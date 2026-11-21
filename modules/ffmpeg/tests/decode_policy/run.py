#!/usr/bin/env python3
"""Exercise opt-in CPU fallback and strict rejection without hiding other errors."""
import argparse
import os
from pathlib import Path
import re
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--godot", type=Path, required=True)
    parser.add_argument("--h264", type=Path, required=True)
    parser.add_argument("--unsupported", type=Path, required=True, help="VP9 fixture (software decoder available, Vulkan bridge unsupported)")
    parser.add_argument("--logs", type=Path, required=True)
    parser.add_argument("--validation", action="store_true")
    parser.add_argument("--compiled-out", action="store_true", help="Test an engine built with ffmpeg_gpu_decode=no")
    args = parser.parse_args()
    args.logs.mkdir(parents=True, exist_ok=True)
    success = True
    scenarios = ("compiled_out", "strict_no_gpu", "software_device") if args.compiled_out else ("gpu", "strict", "software_device")
    for scenario in scenarios:
        command = [
            str(args.godot.resolve()), "--path", str(Path(__file__).resolve().parent),
            "--script", "res://test.gd", "--audio-driver", "Dummy",
            "--rendering-method", "gl_compatibility" if scenario == "software_device" else "forward_plus",
            "--render-thread", "separate", "--verbose",
        ]
        env = os.environ.copy()
        validate = args.validation and scenario != "software_device"
        if validate:
            command.append("--gpu-validation")
            env["VK_LOADER_DEBUG"] = "layer"
            env["VK_LAYER_ENABLES"] = ",".join(filter(None, [env.get("VK_LAYER_ENABLES"), "VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT"]))
        command += ["--", str(args.h264.resolve()), str(args.unsupported.resolve()), scenario]
        try:
            result = subprocess.run(command, env=env, capture_output=True, timeout=90)
        except subprocess.TimeoutExpired:
            print(f"{scenario}: FAIL (timeout)", file=sys.stderr)
            success = False
            continue
        records = result.stdout.decode(errors="replace")
        output = records + "\n--- stderr ---\n" + result.stderr.decode(errors="replace")
        (args.logs / f"{scenario}.log").write_text(output)
        expected_strict_errors = 0
        errors = []
        for line in output.splitlines():
            if "ERROR:" in line and "Could not load global script cache." not in line:
                expected = (
                    scenario == "strict" and line.startswith("ERROR: Vulkan decoding is unavailable for codec vp9")
                ) or (
                    scenario == "strict_no_gpu" and line.startswith("ERROR: This engine was built without Vulkan Video decoding support.")
                )
                if expected:
                    expected_strict_errors += 1
                else:
                    errors.append(line)
        passed = (
            result.returncode == 0
            and f"DECODE_POLICY_TEST scenario={scenario} result=PASS" in records
            and not errors
            and not re.search(r"VUID-|SYNC-HAZARD|Validation Error|result=FAIL", output)
            and (not scenario.startswith("strict") or expected_strict_errors == 2)
            and (not validate or re.search(r'Insert(?:ed device| instance) layer "VK_LAYER_KHRONOS_validation"', output))
        )
        print(f"{scenario}: {'PASS' if passed else 'FAIL'} (see {args.logs / (scenario + '.log')})")
        for error in errors:
            print(error, file=sys.stderr)
        success &= bool(passed)
    return 0 if success else 1


if __name__ == "__main__":
    sys.exit(main())
