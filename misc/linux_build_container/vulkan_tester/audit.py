#!/usr/bin/env python3
"""Reject host-built/non-portable ELF bundles before creating tester archives."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess

PLATFORM = {
    "libc.so.6", "libm.so.6", "libpthread.so.0", "libdl.so.2", "librt.so.1",
    "libresolv.so.2", "libgcc_s.so.1", "ld-linux-x86-64.so.2",
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package", type=Path)
    parser.add_argument("--max-glibc", default="2.35")
    args = parser.parse_args()
    package = args.package.resolve()
    maximum = tuple(map(int, args.max_glibc.split(".")))
    entries = []
    for path in sorted(package.iterdir()):
        if not path.is_file() or path.read_bytes()[:4] != b"\x7fELF":
            continue
        versions = subprocess.check_output(["readelf", "--version-info", str(path)], text=True)
        required = {tuple(map(int, value.split("."))) for value in re.findall(r"\bGLIBC_(\d+(?:\.\d+)+)", versions)}
        if required and max(required) > maximum:
            raise SystemExit(f"{path.name}: requires glibc {'.'.join(map(str, max(required)))}, limit is {args.max_glibc}")
        env = {"PATH": os.environ["PATH"], "LD_LIBRARY_PATH": str(package), "LC_ALL": "C"}
        dependencies = subprocess.check_output(["ldd", str(path)], env=env, text=True)
        if "not found" in dependencies:
            raise SystemExit(f"{path.name}: unresolved dependencies\n{dependencies}")
        for name, resolved in re.findall(r"^\s*(\S+)\s+=>\s+(/\S+)", dependencies, re.MULTILINE):
            if name not in PLATFORM and not Path(resolved).resolve().is_relative_to(package):
                raise SystemExit(f"{path.name}: unbundled dependency {name} ({resolved})")
        entries.append({
            "file": path.name,
            "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            "glibc": ".".join(map(str, max(required))) if required else None,
            "dependencies": dependencies.splitlines(),
        })
    if not any(entry["file"] == "godot-vulkan-tester" for entry in entries):
        raise SystemExit("Missing tester executable")
    (package / "DEPENDENCY-AUDIT.json").write_text(json.dumps(entries, indent=2) + "\n")
    print(f"PASS: audited {len(entries)} ELF files; glibc <= {args.max_glibc}; no unbundled non-platform dependencies")


if __name__ == "__main__":
    main()
