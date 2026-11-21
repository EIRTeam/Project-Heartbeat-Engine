#!/usr/bin/env python3
"""Run the frame-only Vulkan diagnostic; never treat missing validation as a pass."""

import argparse
import os
from pathlib import Path
import re
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--godot", required=True, type=Path)
    parser.add_argument("--video", required=True, type=Path)
    parser.add_argument("--log", required=True, type=Path)
    parser.add_argument("--validation", action="store_true")
    parser.add_argument("--playback", action="store_true")
    parser.add_argument("--external-clock", action="store_true", help="Test music-clock-driven playback")
    parser.add_argument("--software", action="store_true", help="Run the external-clock test on software decoding/OpenGL")
    parser.add_argument("--shutdown", action="store_true", help="Quit playback with frames still in flight")
    parser.add_argument("--render-thread", choices=["safe", "separate"], default="safe")
    parser.add_argument("--color-bars", action="store_true", help="Compare generated reference bars on the GPU")
    parser.add_argument("--skip-second", action="store_true", help="Negative test: leave the second player stopped")
    args = parser.parse_args()
    if args.shutdown and not args.playback:
        parser.error("--shutdown requires --playback")
    if args.software and (not args.external_clock or args.validation):
        parser.error("--software requires --external-clock and cannot use Vulkan validation")
    project = Path(__file__).resolve().parent
    if args.playback:
        project = project.parent / "vulkan_playback"
    if args.external_clock:
        project = Path(__file__).resolve().parent.parent / "music_clock"
    command = [
        str(args.godot.resolve()),
        "--path", str(project),
        "--script", "res://test.gd",
        "--audio-driver", "Dummy",
        "--rendering-method", "gl_compatibility" if args.software else "forward_plus",
        "--render-thread", args.render_thread,
        "--verbose",
    ]
    env = os.environ.copy()
    if args.validation:
        command.append("--gpu-validation")
        env["VK_LOADER_DEBUG"] = "layer"
        enable = "VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT"
        env["VK_LAYER_ENABLES"] = ",".join(filter(None, [env.get("VK_LAYER_ENABLES"), enable]))
    command.extend(["--", str(args.video.resolve())])
    if args.software:
        command.append("software")
    if args.shutdown:
        command.append("shutdown")
    if args.color_bars:
        command.append("color-bars")
    if args.skip_second:
        command.append("skip-second")
    # Keep the streams separate: engine errors on stderr can otherwise land in
    # the middle of a stdout result record and corrupt it.
    try:
        result = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=90)
    except subprocess.TimeoutExpired as error:
        args.log.write_bytes((error.stdout or b"") + b"\n--- stderr ---\n" + (error.stderr or b""))
        print(f"FAIL: diagnostic timed out; see {args.log}", file=sys.stderr)
        return 1
    args.log.write_bytes(result.stdout + b"\n--- stderr ---\n" + result.stderr)
    records = result.stdout.decode(errors="replace")
    output = records + "\n" + result.stderr.decode(errors="replace")
    passed = re.findall(r"VULKAN_DECODE_TEST decoder=(\d+) frames=(\d+) result=PASS", records)
    failures = re.findall(r"(?im)^.*(?:VUID-|SYNC-HAZARD|Validation Error|result=FAIL).*$", output)
    # The unimported test project may emit this unrelated cache warning. Any
    # other engine/script error is a failure, even if the process returns zero.
    engine_errors = [
        line for line in output.splitlines()
        if ("ERROR:" in line or "SCRIPT ERROR:" in line)
        and "Could not load global script cache." not in line
    ]
    layer_loaded = re.search(r'Insert(?:ed device| instance) layer "VK_LAYER_KHRONOS_validation"', output)
    records_valid = (
        len(passed) == 2
        and {index for index, count in passed if int(count) >= 2} == {"0", "1"}
        and "VULKAN_DECODE_TEST result=0" in records
    )
    if args.playback:
        match = re.search(r"VULKAN_PLAYBACK_TEST submitted=(\d+) completed=(\d+)", records)
        records_valid = bool(match and int(match[1]) > 0 and match[1] == match[2])
        if match:
            print(f"GPU conversion jobs: submitted={match[1]} completed={match[2]}")
        players = re.findall(r"VULKAN_PLAYBACK_PLAYER index=(\d+) .* result=PASS", records)
        records_valid = records_valid and sorted(players) == ["0", "1"]
        if args.color_bars:
            colors = re.search(r"VULKAN_PLAYBACK_COLOR checked=(\d+) failed=(\d+)", records)
            records_valid = records_valid and bool(colors and int(colors[1]) >= 16 and int(colors[2]) == 0)
        if args.shutdown:
            match = re.search(r"VULKAN_PLAYBACK_SHUTDOWN submitted=(\d+)", records)
            records_valid = bool(match and int(match[1]) > 0)
    if args.external_clock:
        phases = re.findall(r"VIDEO_CLOCK_PHASE name=(\w+)\b[^\n]* result=PASS", records)
        records_valid = (
            sorted(phases) == sorted(["start", "hitch_and_jitter", "normal", "music_rate", "forward_seek", "backward_seek", "slow_music", "past_end", "loop_after_end", "independent_players", "standalone"])
            and "VIDEO_CLOCK_TEST result=PASS" in records
        )
        for line in records.splitlines():
            if line.startswith("VIDEO_CLOCK_"):
                print(line)
    success = (
        result.returncode == 0
        and records_valid
        and not failures
        and not engine_errors
        and (not args.validation or layer_loaded)
    )
    for index, count in passed:
        print(f"decoder {index}: {count} GPU frames")
    if result.returncode != 0:
        print(f"FAIL: engine exited with code {result.returncode}", file=sys.stderr)
    if args.validation and not layer_loaded:
        print("FAIL: Khronos validation layer was not loaded", file=sys.stderr)
    for line in failures + engine_errors:
        print(line, file=sys.stderr)
    print(f"{'PASS' if success else 'FAIL'}: see {args.log}")
    return 0 if success else 1


if __name__ == "__main__":
    sys.exit(main())
