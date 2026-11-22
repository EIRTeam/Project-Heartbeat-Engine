import os
import re
import shutil
import tarfile
import urllib.request

import SCons

FFMPEG_DOWNLOAD_WIN64 = "https://github.com/EIRTeam/FFmpeg-Builds/releases/download/autobuild-2023-07-24-08-52/ffmpeg-N-111611-g5b11ee9429-win64-lgpl-godot.tar.xz"
FFMPEG_DOWNLOAD_LINUX64 = "https://github.com/EIRTeam/FFmpeg-Builds/releases/download/autobuild-2023-07-24-08-52/ffmpeg-N-111611-g5b11ee9429-linux64-lgpl-godot.tar.xz"
ffmpeg_versions = {
    "avcodec": "60",
    "avfilter": "9",
    "avformat": "60",
    "avutil": "58",
    "swresample": "4",
    "swscale": "7",
}

ffmpeg_version_macros = {
    "avcodec": "LIBAVCODEC_VERSION_MAJOR",
    "avfilter": "LIBAVFILTER_VERSION_MAJOR",
    "avformat": "LIBAVFORMAT_VERSION_MAJOR",
    "avutil": "LIBAVUTIL_VERSION_MAJOR",
    "swresample": "LIBSWRESAMPLE_VERSION_MAJOR",
    "swscale": "LIBSWSCALE_VERSION_MAJOR",
}


def get_ffmpeg_versions(source_dir):
    """Library ABI majors of the FFmpeg build in source_dir, read from its headers.

    Falls back to the pinned download's versions while that is not extracted yet."""
    versions = {}
    for lib, macro in ffmpeg_version_macros.items():
        for header in ("version_major.h", "version.h"):
            try:
                with open(os.path.join(source_dir, "include", f"lib{lib}", header), encoding="utf-8") as version_file:
                    match = re.search(rf"^#define\s+{macro}\s+(\d+)", version_file.read(), re.MULTILINE)
            except OSError:
                continue
            if match:
                versions[lib] = match.group(1)
                break
    if not versions:
        return dict(ffmpeg_versions)
    missing = sorted(set(ffmpeg_version_macros) - set(versions))
    if missing:
        raise RuntimeError(f"Could not determine the ABI version of {', '.join(missing)} from {source_dir}")
    return versions


def get_ffmpeg_install_targets(env, target_dir, source_dir):
    versions = get_ffmpeg_versions(source_dir)
    if env["platform"] == "linuxbsd" or env["platform"] == "linux":
        return [os.path.join(target_dir, f"lib{lib}.so.{version}") for lib, version in versions.items()]
    elif env["platform"] == "android":
        return [os.path.join(target_dir, f"lib{lib}.so") for lib in versions]
    elif env["platform"] == "macos":
        return [os.path.join(target_dir, f"lib{lib}.dylib") for lib in versions]
    else:
        return [os.path.join(target_dir, f"{lib}-{version}.dll") for lib, version in versions.items()]


def get_ffmpeg_install_sources(env, source_dir):
    versions = get_ffmpeg_versions(source_dir)
    if env["platform"] in ("linuxbsd", "linux", "android"):
        return [os.path.join(source_dir, f"lib/lib{lib}.so") for lib in versions]
    elif env["platform"] == "macos":
        return [os.path.join(source_dir, f"lib/lib{lib}.dylib") for lib in versions]
    else:
        return [os.path.join(source_dir, f"bin/{lib}-{version}.dll") for lib, version in versions.items()]


def get_download_url(env):
    if env["platform"] == "linuxbsd" or env["platform"] == "linux":
        FFMPEG_DOWNLOAD_URL = FFMPEG_DOWNLOAD_LINUX64
    else:
        FFMPEG_DOWNLOAD_URL = FFMPEG_DOWNLOAD_WIN64
    return FFMPEG_DOWNLOAD_URL


def download_ffmpeg(target, source, env):
    dst = ""
    if isinstance(target[0], str):
        dst = os.path.dirname(target[0])
    else:
        dst = os.path.dirname(target[0].get_path())
    if os.path.exists(dst):
        shutil.rmtree(dst)

    FFMPEG_DOWNLOAD_URL = get_download_url(env)

    local_filename, headers = urllib.request.urlretrieve(FFMPEG_DOWNLOAD_URL)

    def rewrite_subfolder_paths(tf, common_path):
        common_path_length = len(common_path)
        for member in tf.getmembers():
            if member.path.startswith(common_path):
                member.path = member.path[common_path_length:]
                yield member

    with tarfile.open(local_filename, mode="r") as f:
        # Get the first folder
        common_path = os.path.commonpath(f.getnames()) + "/"
        f.extractall(dst, members=rewrite_subfolder_paths(f, common_path))
    os.remove(local_filename)


def _ffmpeg_emitter(target, source, env):
    dst = ""
    if isinstance(target[0], str):
        dst = os.path.dirname(target[0])
    else:
        dst = os.path.dirname(target[0].get_path())
    versions = get_ffmpeg_versions(dst)
    target += get_ffmpeg_install_sources(env, dst)
    if env["platform"] == "windows":
        target += [os.path.join(dst, f"lib/{lib}.lib") for lib in versions]
    else:
        target += [os.path.join(dst, f"lib/lib{lib}.so") for lib in versions]

    emitter_headers = [
        "libavcodec/codec.h",
        "libavcodec/avcodec.h",
        "libavutil/frame.h",
        "libavformat/avformat.h",
        "libavformat/avio.h",
        "libswresample/swresample.h",
        "libswscale/swscale.h",
    ]

    target += [os.path.join(dst, "include/" + x) for x in emitter_headers]

    return target, source


def ffmpeg_download_builder(env, target, source):
    bkw = {
        "action": env.Run(download_ffmpeg),
        "target_factory": env.fs.Entry,
        "source_factory": env.fs.Entry,
        "emitter": _ffmpeg_emitter,
    }

    bld = SCons.Builder.Builder(**bkw)
    return bld(env, target, source)


def ffmpeg_install(env, target, source):
    return env.InstallAs(get_ffmpeg_install_targets(env, target, source), get_ffmpeg_install_sources(env, source))
