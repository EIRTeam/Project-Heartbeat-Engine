# Portable Linux Vulkan-video tester build

Builds an opt-in tester archive on the Ubuntu 22.04 baseline (glibc 2.35),
independent of the host toolchain and of host build objects.

```sh
docker build -t ph-vulkan-tester-builder:ubuntu22 misc/linux_build_container/vulkan_tester
mkdir -p /tmp/opencode/vulkan-portable-artifacts
docker run --rm -e JOBS=8 \
    -v "$PWD":/src:ro -v /tmp/opencode/vulkan-portable-artifacts:/out \
    ph-vulkan-tester-builder:ubuntu22
```

The build script:

- snapshots `engine`, `modules` and `misc` from the read-only checkout,
  including uncommitted changes but excluding `engine/bin` and build caches;
- builds pinned FFmpeg 9.1 (`74e4393`) with `--disable-autodetect`, so it
  cannot pick up libva, VDPAU, OpenCL, Opus or Vorbis from the build image;
  the only hardware acceleration is H.264/HEVC Vulkan decode;
- builds the template with `ffmpeg_path`, automatic GPU support, and `ffmpeg_gpu_tests=yes`, and
  without Steamworks (that SDK is not redistributable here);
- bundles FFmpeg libraries under their SONAMEs next to the binary (`$ORIGIN`
  RPATH), plus licenses, build/config metadata and the diagnostic projects;
- runs `audit.py`, which **fails the build** if any ELF requires glibc newer
  than 2.35 or resolves a non-platform library from outside the package, and
  writes `DEPENDENCY-AUDIT.json` with hashes and resolved dependencies.

Output: `vulkan-tester-linux-x86_64.tar.gz` and its `.sha256`.

FFmpeg's native Opus/Vorbis decoders are used instead of libopus/libvorbis.
The archive has not been run on every distribution; distribute it as an
explicitly opt-in alpha and collect `vulkaninfo --summary`, driver version and
the runner log from each tester. GPU output stays off unless both
`video_decode` and `video_decode_output` project settings are enabled.
