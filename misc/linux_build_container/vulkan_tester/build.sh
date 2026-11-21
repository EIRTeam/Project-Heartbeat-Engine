#!/bin/bash
set -euo pipefail

# Mount the checkout read-only at /src and an empty artifact directory at /out.
# Build a snapshot elsewhere, including uncommitted changes but no host objects.
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
mkdir -p "$work/source" "$work/ffmpeg" "$work/prefix" /out
tar -C /src --exclude=.git --exclude=engine/bin --exclude='*.o' \
    --exclude='.scons*' --exclude='__pycache__' --exclude=buildeps \
    -cf - engine modules misc | tar -C "$work/source" -xf -
jobs="${JOBS:-8}"
ffmpeg_commit=74e4393595cf2b5857f0da925c4e0dd74ea306e5
curl -fL "https://github.com/FFmpeg/FFmpeg/archive/${ffmpeg_commit}.tar.gz" -o "$work/ffmpeg.tar.gz"
tar -xzf "$work/ffmpeg.tar.gz" --strip-components=1 -C "$work/ffmpeg"
cd "$work/ffmpeg"
./configure --cc=gcc --prefix="$work/prefix" \
    --extra-cflags="-I$work/source/engine/thirdparty/vulkan/include" \
    --enable-version3 --disable-debug --disable-autodetect --enable-vulkan \
    --enable-shared --disable-static --disable-programs --disable-doc --disable-network \
    --disable-avdevice --disable-encoders --disable-muxers --disable-filters \
    --disable-decoders --enable-decoder=h264,hevc,vp8,vp9,av1,mpeg4,aac,aac_latm,mp3,vorbis,opus \
    --disable-parsers --enable-parser=h264,hevc,vp8,vp9,av1,mpeg4video,mpegaudio,aac,aac_latm,opus,vorbis \
    --disable-demuxers --enable-demuxer=matroska,mov,aac,flv,avi \
    --disable-hwaccels --enable-hwaccel=h264_vulkan,hevc_vulkan
make -j"$jobs"
make install
cd "$work/source/engine"
scons -j"$jobs" platform=linuxbsd target=template_release \
    custom_modules=../modules module_steamworks_enabled=no \
    ffmpeg_path="$work/prefix" \
    ffmpeg_gpu_tests=yes disable_path_overrides=no \
    accesskit=no wayland=no debug_symbols=no
package="$work/package"
mkdir -p "$package/licenses" "$package/tests"
cp bin/godot.linuxbsd.template_release.x86_64 "$package/godot-vulkan-tester"
for library in "$work/prefix"/lib/lib*.so; do
    soname="$(readelf -d "$library" | awk -F '[][]' '/SONAME/ {print $2}')"
    test -n "$soname"
    cp -L "$library" "$package/$soname"
done
cp LICENSE.txt COPYRIGHT.txt "$package/licenses/"
cp ../modules/ffmpeg/LICENSE "$package/licenses/FFmpeg-module-MIT.txt"
cp "$work/ffmpeg/COPYING.LGPLv3" "$package/licenses/FFmpeg-LGPLv3.txt"
cp "$work/ffmpeg/COPYING.GPLv3" "$package/licenses/FFmpeg-GPLv3-reference.txt"
cp -a ../modules/ffmpeg/tests/vulkan_decode ../modules/ffmpeg/tests/vulkan_playback ../modules/ffmpeg/tests/music_clock "$package/tests/"
cp ../misc/linux_build_container/vulkan_tester/TESTING.md "$package/"
printf '%s\n' "FFmpeg commit: $ffmpeg_commit" "Build baseline: Ubuntu 22.04 (glibc 2.35)" \
    "Engine sources: supplied checkout snapshot" > "$package/BUILD-INFO.txt"
cp "$work/ffmpeg/ffbuild/config.mak" "$package/FFMPEG-CONFIG.txt"
python3 /src/misc/linux_build_container/vulkan_tester/audit.py "$package" --max-glibc 2.35
"$package/godot-vulkan-tester" --version
tar -C "$package" -czf /out/vulkan-tester-linux-x86_64.tar.gz .
(cd /out && sha256sum vulkan-tester-linux-x86_64.tar.gz > vulkan-tester-linux-x86_64.tar.gz.sha256)
