# Renderer and video upgrade smoke test

Requires the PH engine, FFmpeg CLI, and a working display/rendering driver.
Run from this directory:

```bash
ffmpeg -f lavfi -i color=c=red:s=160x96:r=24 -t 2 -an \
  -c:v mpeg4 -pix_fmt yuv420p red.mp4

/path/to/ph-engine --path "$PWD" --rendering-method mobile \
  --rendering-driver vulkan --audio-driver Dummy --script smoke.gd

/path/to/ph-engine --path "$PWD" --rendering-method gl_compatibility \
  --rendering-driver opengl3 --audio-driver Dummy --script smoke.gd
```

Success requires exit 0 and `PH_GPU_MEDIA_PASS`, with no engine/script errors.
The fixture verifies clipping pixels, Compositor2D callback/texture validity
on RenderingDevice renderers, decoded video colors, seeking, and three
playback/teardown cycles. It does not contact services or use game assets.

Validated using an isolated Xvfb display and Mesa llvmpipe/lavapipe. This
exercises the GL and Vulkan code paths, not physical GPU performance or
display/audio latency. Do not use `--headless`, which selects the dummy
renderer instead of testing these paths.
