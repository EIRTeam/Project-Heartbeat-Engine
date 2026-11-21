# config.py


def can_build(env, platform):
    return ((env["platform"] == "linuxbsd" or env["platform"] == "windows") and env["arch"] == "x86_64") or env[
        "platform"
    ] == "android"


def configure(env):
    if "ffmpeg_path" not in env or not env["ffmpeg_path"]:
        raise RuntimeError("ffmpeg path not found")


def get_opts(platform):
    return [
        ("ffmpeg_path", "FFmpeg path", ""),
        (
            "ffmpeg_gpu_decode",
            "Compile Vulkan Video decoding support (auto, yes, no). auto enables it when ffmpeg_path has FFmpeg 9 or newer",
            "auto",
        ),
        ("ffmpeg_gpu_tests", "Expose the GPU-resident decode diagnostics (engine module only)", "no"),
    ]


def get_doc_path():
    return "doc_classes"
