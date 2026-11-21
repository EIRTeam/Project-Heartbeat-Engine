"""Run with python3 -m unittest discover -s modules/ffmpeg/tests."""

import importlib.util
from pathlib import Path
import sys
import tempfile
import types
import unittest


MODULE = Path(__file__).resolve().parents[1]
# The ABI helpers do not need SCons itself.
sys.modules.setdefault("SCons", types.ModuleType("SCons"))
spec = importlib.util.spec_from_file_location("ffmpeg_download", MODULE / "gdextension_build/ffmpeg_download.py")
download = importlib.util.module_from_spec(spec)
spec.loader.exec_module(download)


class BuildConfigurationTests(unittest.TestCase):
    def test_header_versions_and_install_paths(self):
        with tempfile.TemporaryDirectory() as directory:
            versions = dict(avcodec="63", avfilter="12", avformat="63", avutil="61", swresample="7", swscale="10")
            for index, (lib, macro) in enumerate(download.ffmpeg_version_macros.items()):
                header = Path(directory) / "include" / f"lib{lib}" / ("version.h" if index % 2 else "version_major.h")
                header.parent.mkdir(parents=True)
                header.write_text(f"#define {macro} {versions[lib]}\n")
            self.assertEqual(download.get_ffmpeg_versions(directory), versions)
            for gpu in ("auto", "yes", "no"):
                env = dict(platform="windows", ffmpeg_gpu_decode=gpu)
                self.assertIn("bin/avcodec-63.dll", download.get_ffmpeg_install_targets(env, "bin", directory))
            self.assertIn("bin/libavutil.so.61", download.get_ffmpeg_install_targets(dict(platform="linuxbsd"), "bin", directory))
            (Path(directory) / "include/libavutil/version.h").unlink()
            with self.assertRaises(RuntimeError):
                download.get_ffmpeg_versions(directory)

    def test_legacy_download_before_extraction(self):
        with tempfile.TemporaryDirectory() as directory:
            self.assertEqual(download.get_ffmpeg_versions(directory), download.ffmpeg_versions)

    def configure_gpu(self, major=61, **options):
        class Environment(dict):
            def Append(self, **values):
                for key, value in values.items():
                    self.setdefault(key, []).extend(value)

        env = dict(platform="linuxbsd", arch="x86_64", vulkan=True, **options)
        local = dict(
            env=env, env_ffmpeg=Environment(), ffmpeg_path="/prefix",
            ffmpeg_download=types.SimpleNamespace(get_ffmpeg_versions=lambda _: {"avutil": str(major)}),
            print_info=lambda _: None,
        )
        # Exercise the actual SCsub decision block without a full engine build.
        script = (MODULE / "SCsub").read_text()
        script = script[script.index('gpu_decode = env.get('):script.index('# This define is intentionally')]
        exec(compile(script, str(MODULE / "SCsub"), "exec"), local)
        return local["gpu_decode_enabled"]

    def test_gpu_auto_and_overrides(self):
        self.assertTrue(self.configure_gpu())
        self.assertFalse(self.configure_gpu(major=58))
        self.assertFalse(self.configure_gpu(ffmpeg_gpu_decode="no"))
        self.assertTrue(self.configure_gpu(ffmpeg_gpu_decode="yes"))
        self.assertTrue(self.configure_gpu(ffmpeg_gpu_tests="yes"))
        for options in (
            dict(major=58, ffmpeg_gpu_decode="yes"),
            dict(ffmpeg_gpu_decode="invalid"),
            dict(ffmpeg_gpu_decode="no", ffmpeg_gpu_tests="yes"),
        ):
            with self.assertRaises(RuntimeError):
                self.configure_gpu(**options)


if __name__ == "__main__":
    unittest.main()
