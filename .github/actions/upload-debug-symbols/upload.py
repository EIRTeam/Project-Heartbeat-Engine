#!/usr/bin/env python3
"""Upload build symbols using a pinned, checksum-verified Sentry CLI."""

import glob
import hashlib
import json
import os
import subprocess
import sys
import tempfile
import urllib.request
from pathlib import Path

CLI_VERSION = "3.8.2"
# SHA-256 digests published with the upstream release assets.
CLI_ASSETS = {
    "Linux": (
        "sentry-cli-Linux-x86_64",
        "c958865f9d4000183509ea21b1f04925d5390de6d8d87386b9e69f2b305070c3",
    ),
    "Windows": (
        "sentry-cli-Windows-x86_64.exe",
        "50009f23c49d6e9ff7624204c7035855303b49c5e326708ee783c1fdf783ac83",
    ),
}


def find_symbols(pattern):
    paths = sorted(Path(path).resolve() for path in glob.glob(pattern))
    if not paths or any(not path.is_file() or path.stat().st_size == 0 for path in paths):
        raise RuntimeError(f"No nonempty symbol files matched {pattern!r}")
    return paths


def download_cli(directory, runner_os, runner_arch):
    if runner_arch != "X64" or runner_os not in CLI_ASSETS:
        raise RuntimeError(f"Unsupported runner: {runner_os}/{runner_arch}")
    asset, expected_digest = CLI_ASSETS[runner_os]
    url = f"https://github.com/getsentry/sentry-cli/releases/download/{CLI_VERSION}/{asset}"
    destination = directory / asset
    digest = hashlib.sha256()
    with urllib.request.urlopen(url, timeout=60) as response, destination.open("wb") as output:
        while chunk := response.read(1024 * 1024):
            digest.update(chunk)
            output.write(chunk)
    if digest.hexdigest() != expected_digest:
        raise RuntimeError(f"SHA-256 mismatch for {asset}; refusing to execute it")
    destination.chmod(0o700)
    return destination


def validate_symbols(cli, paths):
    for path in paths:
        result = subprocess.run(
            [str(cli), "debug-files", "check", "--json", str(path)],
            check=True,
            capture_output=True,
            text=True,
        )
        info = json.loads(result.stdout)
        features = {feature.strip() for feature in info.get("features", "").split(",")}
        if not info.get("is_usable") or "debug" not in features:
            raise RuntimeError(f"Missing usable debugging information in {path}")
        print(f"Validated debugging information: {path.name}", flush=True)


def main():
    # Defense in depth: callers must also gate the action before exposing its secret.
    if os.environ.get("GITHUB_REF") != "refs/heads/master" or os.environ.get("GITHUB_EVENT_NAME") not in (
        "push",
        "workflow_dispatch",
    ):
        raise RuntimeError("Symbol uploads are only allowed for master pushes or manual master builds")
    if not os.environ.get("SENTRY_AUTH_TOKEN", "").strip():
        raise RuntimeError("GLITCHTIP_AUTH_TOKEN is required for master symbol uploads")
    paths = find_symbols(os.environ["SYMBOL_PATH_PATTERN"])
    with tempfile.TemporaryDirectory(prefix="sentry-cli-", dir=os.environ.get("RUNNER_TEMP")) as directory:
        cli = download_cli(Path(directory), os.environ["RUNNER_OS"], os.environ["RUNNER_ARCH"])
        validate_symbols(cli, paths)
        # Keep the auth token in the environment, never in command-line arguments.
        subprocess.run(
            [str(cli), "debug-files", "upload", "--wait-for", "600", *map(str, paths)],
            check=True,
        )


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, ValueError, subprocess.CalledProcessError) as error:
        print(f"::error::{error}", file=sys.stderr)
        sys.exit(1)
