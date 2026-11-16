import hashlib
import io
import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import upload


class SymbolUploadTests(unittest.TestCase):
    def test_find_symbols_expands_globs_including_paths_with_spaces(self):
        with tempfile.TemporaryDirectory(prefix="symbol test ") as directory:
            path = Path(directory) / "godot.windows.editor.x86_64.pdb"
            path.write_bytes(b"symbols")
            self.assertEqual(upload.find_symbols(str(Path(directory) / "*.pdb")), [path.resolve()])

    def test_missing_empty_or_directory_inputs_fail(self):
        with tempfile.TemporaryDirectory() as directory:
            empty = Path(directory) / "empty.pdb"
            empty.touch()
            for pattern in (str(Path(directory) / "missing*"), str(empty), directory):
                with self.subTest(pattern=pattern), self.assertRaises(RuntimeError):
                    upload.find_symbols(pattern)

    def test_download_verifies_checksum_for_both_runners(self):
        data = b"test cli"
        checksum = hashlib.sha256(data).hexdigest()
        for runner_os, asset in (("Linux", "cli"), ("Windows", "cli.exe")):
            with self.subTest(runner_os=runner_os), tempfile.TemporaryDirectory() as directory:
                with (
                    mock.patch.dict(upload.CLI_ASSETS, {runner_os: (asset, checksum)}),
                    mock.patch.object(upload.urllib.request, "urlopen", return_value=io.BytesIO(data)),
                ):
                    cli = upload.download_cli(Path(directory), runner_os, "X64")
                self.assertEqual(cli.read_bytes(), data)

    def test_bad_checksum_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            with mock.patch.object(upload.urllib.request, "urlopen", return_value=io.BytesIO(b"tampered")):
                with self.assertRaisesRegex(RuntimeError, "SHA-256 mismatch"):
                    upload.download_cli(Path(directory), "Linux", "X64")

    def test_unsupported_runners_fail_before_downloading(self):
        for runner_os, runner_arch in (("macOS", "X64"), ("Linux", "ARM64")):
            with self.subTest(runner_os=runner_os), mock.patch.object(upload.urllib.request, "urlopen") as download:
                with self.assertRaisesRegex(RuntimeError, "Unsupported runner"):
                    upload.download_cli(Path("unused"), runner_os, runner_arch)
                download.assert_not_called()

    def test_symbol_validation_requires_usable_debug_information(self):
        for usable, features, valid in (
            (True, "symtab, debug, unwind", True),
            (True, "symtab, unwind", False),
            (False, "debug", False),
        ):
            with self.subTest(usable=usable, features=features):
                result = subprocess.CompletedProcess([], 0, json.dumps({"is_usable": usable, "features": features}))
                with mock.patch.object(upload.subprocess, "run", return_value=result):
                    if valid:
                        upload.validate_symbols(Path("cli"), [Path("symbols")])
                    else:
                        with self.assertRaisesRegex(RuntimeError, "Missing usable debugging information"):
                            upload.validate_symbols(Path("cli"), [Path("symbols")])

    def test_branch_and_event_gate_prevents_downloads(self):
        events = ("push", "workflow_dispatch", "pull_request", "pull_request_target", "merge_group", "release")
        refs = ("refs/heads/master", "refs/heads/feature", "refs/tags/master", "refs/pull/1/merge")
        for event in events:
            for ref in refs:
                with self.subTest(event=event, ref=ref), mock.patch.dict(
                    os.environ, {"GITHUB_EVENT_NAME": event, "GITHUB_REF": ref}, clear=True
                ), mock.patch.object(upload, "download_cli") as download:
                    allowed = event in ("push", "workflow_dispatch") and ref == "refs/heads/master"
                    message = "GLITCHTIP_AUTH_TOKEN" if allowed else "only allowed"
                    with self.assertRaisesRegex(RuntimeError, message):
                        upload.main()
                    download.assert_not_called()

    def test_upload_waits_for_processing_and_keeps_token_out_of_arguments(self):
        with tempfile.TemporaryDirectory() as directory:
            symbols = Path(directory) / "engine.elf"
            symbols.write_bytes(b"symbols")
            environment = {
                "GITHUB_REF": "refs/heads/master",
                "GITHUB_EVENT_NAME": "push",
                "SENTRY_AUTH_TOKEN": "test-secret-token",
                "SYMBOL_PATH_PATTERN": str(symbols),
                "RUNNER_OS": "Linux",
                "RUNNER_ARCH": "X64",
            }
            with (
                mock.patch.dict(os.environ, environment, clear=True),
                mock.patch.object(upload, "download_cli", return_value=Path("cli")),
                mock.patch.object(upload, "validate_symbols") as validate,
                mock.patch.object(upload.subprocess, "run") as run,
            ):
                upload.main()
                validate.assert_called_once_with(Path("cli"), [symbols.resolve()])
                run.assert_called_once_with(
                    ["cli", "debug-files", "upload", "--wait-for", "600", str(symbols.resolve())], check=True
                )
                self.assertNotIn(environment["SENTRY_AUTH_TOKEN"], str(run.call_args))
                run.side_effect = subprocess.CalledProcessError(1, ["cli"])
                with self.assertRaises(subprocess.CalledProcessError):
                    upload.main()


if __name__ == "__main__":
    unittest.main()
