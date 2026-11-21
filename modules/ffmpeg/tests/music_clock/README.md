# Music-clock regression

Create a >=6-second, 30fps SDR fixture with both video and audio, for example:

```sh
ffmpeg -f lavfi -i testsrc2=size=320x180:rate=30 \
  -f lavfi -i sine=frequency=880:sample_rate=48000 -t 12 \
  -c:v libx264 -pix_fmt yuv420p -g 30 -c:a aac /tmp/opencode/music-clock.mp4
```

Run GPU playback (use a Vulkan Video-capable build/device):

```sh
python3 modules/ffmpeg/tests/vulkan_decode/run.py \
  --godot engine/bin/godot.linuxbsd.template_release.x86_64 \
  --video /tmp/opencode/music-clock.mp4 --external-clock \
  --render-thread separate --validation --log /tmp/opencode/music-clock.log
```

Use `--software` instead of `--validation` to exercise software decoding/OpenGL.
The clock API does not require `ffmpeg_gpu_tests=yes`; the existing GPU color/
frame diagnostics still do.

Checks cover negative pre-song time, frozen non-frame-aligned targets, normal
playback, music speed differing from `VideoStreamPlayer.speed_scale`, forward/
backward seeks, slow playback, holding beyond EOF, looping back after EOF, and
bounded paused queues without repeated seeks. Timing thresholds refer to frame
selection, not physical display or speaker latency. It also checks independent
clocks on two players and switching back to standalone playback. This is not a substitute
for measuring A/V synchronization on real game content.

A 700 ms game-thread stall and a 20 ms backward music-clock correction must
recover without stopping or unnecessarily seeking. The test also checks the
configurable recovery tolerance.
