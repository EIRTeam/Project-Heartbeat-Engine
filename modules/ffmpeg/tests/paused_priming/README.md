# Paused frame priming regression

Generate a moving, >=6-second 30fps fixture with embedded audio:

```sh
ffmpeg -f lavfi -i testsrc2=size=320x180:rate=30 \
  -f lavfi -i sine=frequency=880:sample_rate=48000 -t 6 \
  -c:v libx264 -pix_fmt yuv420p -g 30 -c:a aac /tmp/opencode/paused-priming.mp4
```

Run with an engine-module build and a display (not `--headless`, since the test
reads back the displayed texture):

```sh
engine/bin/godot.linuxbsd.editor.dev.x86_64 \
  --path modules/ffmpeg/tests/paused_priming --script res://test.gd \
  --audio-driver Dummy --rendering-method gl_compatibility \
  -- /tmp/opencode/paused-priming.mp4
```

Append `automatic` after the fixture path to test
`VideoStreamPlayer.process_while_paused` without manual playback updates.
The backend-independent property regression can also run headlessly:

```sh
engine/bin/godot.linuxbsd.editor.dev.x86_64 --headless \
  --path modules/ffmpeg/tests/paused_priming --script res://player_processing.gd
```

For Vulkan Video, use `--rendering-method forward_plus`
and append `gpu` after the fixture path. A compatible device/build is required.
The safe rendering-thread mode passes; `--render-thread separate` currently
exposes the existing off-main-thread `VideoStreamPlayer.queue_redraw()` callback
and can fail the visible-pixels checks.

Checks cover visible initial priming, forward/backward non-frame-aligned seeks,
frozen time even with nonzero update delta, bounded queues, no external-clock
sampling/recovery, paused EOF, resume, and stopped updates. Success prints
`PAUSED_PRIMING_TEST result=PASS` and exits with code zero.
