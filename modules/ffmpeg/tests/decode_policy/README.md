# Decode backend/policy regression

Build with `ffmpeg_gpu_tests=yes`. Provide a normal SDR H.264 fixture of at least
two seconds and a VP9 fixture (software decoding available, GPU bridge currently
unsupported). For example, generate VP9 with a host FFmpeg:

```sh
ffmpeg -f lavfi -i testsrc2=size=320x180:rate=30 -t 3 \
  -c:v libvpx-vp9 -b:v 0 -crf 35 -an /tmp/opencode/policy-vp9.webm
python3 modules/ffmpeg/tests/decode_policy/run.py \
  --godot engine/bin/godot.linuxbsd.template_release.x86_64 \
  --h264 /tmp/opencode/music-clock.mp4 \
  --unsupported /tmp/opencode/policy-vp9.webm \
  --logs /tmp/opencode/decode-policy-tests --validation
```

The runner checks:
- Explicit software-only decoding on a GPU-capable device, with no fallback reason.
- Unsupported-codec fallback with a nonempty reason.
- Project-default software behavior when global GPU output is disabled.
- H.264 Vulkan backend reporting, not just GPU texture/conversion detection.
- Simulated GPU conversion failure, software restart at the current music-clock
  time, continued playback, and replacement of the player's cached GPU texture.
- Strict/default rejection of unsupported GPU codecs (only those two expected
  engine errors are allowed in the strict test).
- Software fallback when no RenderingDevice exists, using OpenGL, including
  stop/play of RGBA software output.

`--validation` requires a loaded Khronos layer and no Vulkan/synchronization
errors in the Vulkan cases. The software-device case uses OpenGL instead.
The failure-injection API only exists in diagnostic builds; it sets a failure
flag without issuing invalid GPU commands or simulating rendering-device loss.

For a build with `ffmpeg_gpu_decode=no ffmpeg_gpu_tests=no`, add
`--compiled-out`. It checks software fallback and strict rejection when the
engine has no GPU decode implementation at all.
