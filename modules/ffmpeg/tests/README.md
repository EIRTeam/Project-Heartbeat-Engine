# GPU converter lifetime regressions

The C++ tests run with the dummy renderer and deterministically leave clear
commands pending while the playback-side owner is released:

```sh
engine/bin/godot.linuxbsd.editor.dev.x86_64 --headless --test --test-case='[FFmpeg]*'
```

For GPU coverage, run the stress script from a Godot project with a display and
YUV420P and YUVA420P videos (use absolute paths):

```sh
engine/bin/godot.linuxbsd.editor.dev.x86_64 \
  --path /path/to/project --rendering-method mobile --render-thread separate \
  --script /absolute/path/to/modules/ffmpeg/tests/stress_gpu_converter.gd \
  -- /path/to/yuv420p.webm /path/to/yuva420p.webm
```

Repeat with `--render-thread safe` for execution on the main/render thread.
Use an ASan build and Vulkan validation where available. The script alternates
release during playback, stop/clear/release, and stop/restart/release after seeks,
retaining the public output wrapper across teardown and sampling the cleared
output on selected iterations. It must finish without
invalid-object, wrong-thread, invalid-RID, sanitizer, or GPU-validation errors.
Also check normal playback colors/alpha.
