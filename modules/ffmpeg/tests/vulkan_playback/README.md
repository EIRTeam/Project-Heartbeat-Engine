# Synchronized GPU playback

Run via the adjacent diagnostic runner:

```sh
python3 modules/ffmpeg/tests/vulkan_decode/run.py \
    --godot engine/bin/godot.linuxbsd.template_release.x86_64 \
    --video /tmp/opencode/vulkan-test.mp4 --log /tmp/opencode/vulkan-playback.log \
    --validation --playback --render-thread separate
```

Build with `ffmpeg_gpu_decode=yes ffmpeg_gpu_tests=yes` and a Vulkan-enabled
FFmpeg prefix as described in the module README. A display is required.

Two video players display GPU-decoded frames, seek to the beginning, then stop
and free their resources. Each player is checked **individually** through its
own output texture: it must submit and retire conversions, its playback
position and decoded presentation timestamps must advance, and it must present
new, earlier frames after seeking. All conversion jobs must retire after their
RD fences. Add `--shutdown` to quit early with live players instead.
`--skip-second` is a negative self-test that must fail.

## Reference color bars

`create_color_fixtures.py` writes eight-bar YUV fixtures whose codes are
computed from the BT.601/BT.709 equations: 8-bit and 10-bit, limited and full
range. It encodes them offline with a host FFmpeg, then decodes them again and
refuses to emit a fixture whose YUV codes drifted.

```sh
python3 modules/ffmpeg/tests/vulkan_playback/create_color_fixtures.py \
    --output /tmp/opencode/vulkan-color-fixtures
python3 modules/ffmpeg/tests/vulkan_decode/run.py \
    --godot engine/bin/godot.linuxbsd.template_release.x86_64 \
    --video /tmp/opencode/vulkan-color-fixtures/bars-10-full-smpte170m.mp4 \
    --log /tmp/opencode/vulkan-color.log --validation --playback --color-bars
```

With `--color-bars`, a test-only compute pass samples the center of every bar
in each converted output texture on the GPU and compares it with independent
RGB reference values (tolerance 5/255). Only two counters are read back; image
pixels never reach CPU memory. Running `--color-bars` against a non-bar video
is a negative self-test that must fail.

Supported conversion inputs are multiplanar NV12 and P010 SDR images in the
declared concurrent sharing families, decoded into images distinct from the
decoder's reference pictures. Unsupported inputs fail with an error, never
with a CPU pixel fallback. The output is an RD-owned RGBA texture.

All checks use explicit exit codes, so they work where GDScript `assert()` is
disabled. The runner rejects validation errors and refuses to claim validation
coverage when the Khronos layer did not load. The color test covers bar
centers only; it does not test chroma siting at edges, scaling, HDR, or
presentation color management.
