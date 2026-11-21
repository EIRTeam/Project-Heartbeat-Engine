# Vulkan video decoding — alpha tester guide

This build decodes H.264/HEVC video on the GPU with Vulkan Video and displays
it without copying frames through the CPU. It is **experimental and opt-in**.

## Requirements

- Linux x86_64 with glibc 2.35 or newer (Ubuntu 22.04, Debian 12, Fedora 36+,
  or similar), Python 3 for the self-test runner.
- A Vulkan 1.3 driver that exposes `VK_KHR_video_decode_h264`/`h265` on a
  dedicated decode queue. Check with `vulkaninfo --summary`.
- A display session (X11 or XWayland). Headless runs are rejected.

Known working: AMD RX 7800 XT, Mesa RADV 26.2. Everything else is untested.

## Self-tests

Extract the archive and verify it first:

```sh
sha256sum -c vulkan-tester-linux-x86_64.tar.gz.sha256
mkdir vulkan-tester && tar -xzf vulkan-tester-linux-x86_64.tar.gz -C vulkan-tester
cd vulkan-tester
```

Create test videos (needs a system `ffmpeg` with libx264 and libx265; this is
only used to create inputs):

```sh
python3 tests/vulkan_playback/create_color_fixtures.py --output fixtures
```

Run the checks; each prints `PASS` or `FAIL` and writes a log:

```sh
R=tests/vulkan_decode/run.py
python3 $R --godot ./godot-vulkan-tester --video fixtures/bars-8-limited-bt709.mp4 --log decode.log
for f in fixtures/bars-*.mp4; do
  python3 $R --godot ./godot-vulkan-tester --video "$f" --log "$(basename "$f" .mp4).log" --playback --color-bars
done
python3 $R --godot ./godot-vulkan-tester --video fixtures/bars-8-limited-bt709.mp4 --log threaded.log --playback --render-thread separate
```

If the Khronos validation layer is installed (`vulkan-validation-layers`
package), add `--validation` to any command for deeper checking.

## Using it in a project

Add to `project.godot` (both are required, and take effect on restart):

```ini
[rendering]
renderer/rendering_method="forward_plus"
rendering_device/vulkan/video_decode=true
rendering_device/vulkan/video_decode_output=true
```

Supported: progressive 8/10-bit 4:2:0 SDR H.264/HEVC, BT.601/BT.709, full or
limited range. HDR, BT.2020, other codecs, cropped or interlaced video are
**rejected with an error**, not played in software. With
`video_decode_output` off, playback uses the normal software path.

## What to report

Please send, for every run (pass or fail):

- `vulkaninfo --summary` output and your distribution/kernel version;
- the `.log` files written by the runner;
- for project testing, the game log with `--verbose`, and whether video looked
  correct (colors, no flicker, no stutter, seeking works).

Hangs, GPU resets ("device lost") and crashes are the most important reports.
