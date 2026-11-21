# Vulkan frame-only diagnostic

This is **not GPU playback**. It imports the engine's device on the caller
thread, then runs two independent FFmpeg decoders on worker threads. Each
decoder must receive Vulkan frames, retain up to four, wait for their GPU
timeline values, drain, seek back to the start, and decode again. A third pass
stops after receiving frames without draining, exercising mid-stream teardown. Both workers
join and release their frames/codec/device references before returning.
It performs no pixel mapping, download, CPU conversion, or texture upload.
Normal playback is unchanged.
The test skips `avformat_find_stream_info()` to avoid its software-frame
probing; use a container with sufficient codec metadata (such as MP4).

Build the engine module with:

```
ffmpeg_gpu_tests=yes ffmpeg_path=/path/to/ffmpeg
```

The diagnostic API is absent unless `ffmpeg_gpu_tests=yes`. Use a build with
GDScript and `disable_path_overrides=no`. A Vulkan-capable display is required;
`--headless` cannot exercise this bridge.

Create a short, ordinary H.264 fixture using a host FFmpeg with libx264:

```sh
ffmpeg -f lavfi -i testsrc=size=320x240:rate=30 -t 2 \
    -c:v libx264 -pix_fmt yuv420p -g 30 -an /tmp/opencode/vulkan-test.mp4
python3 modules/ffmpeg/tests/vulkan_decode/run.py \
    --godot engine/bin/godot.linuxbsd.template_release.x86_64 \
    --video /tmp/opencode/vulkan-test.mp4 --log /tmp/opencode/vulkan-test.log \
    --validation
```

The runner requires two decoder PASS records, a successful process exit,
no Vulkan validation errors, and an actually loaded Khronos validation layer
when `--validation` is specified. It enables synchronization validation too.
For a locally extracted validation-layer package, set `VK_LAYER_PATH` to its
manifest directory and `LD_LIBRARY_PATH` to its library directory. Without
`--validation`, the test exercises decoding but makes no validation claim.

Unsupported codecs/devices fail rather than falling back to software. The
bridge currently requires a dedicated video-decode family and a compute/transfer
family with at least two queues: queue 0 is reserved for FFmpeg and queue 1 remains
available to the renderer. All imported queues use engine-owned submission
mutexes shared across decoder contexts.

## Investigated initialization crash

An import listing only a video queue can pass `av_hwdevice_ctx_init()` but is
not sufficient for stock FFmpeg 9.1. `vulkan_frames_init()` unconditionally creates
compute and transfer execution pools. A decode-only queue list leaves
`compute_qf` and `transfer_qf` null and crashes in `ff_vk_exec_pool_init()`.
Supplying real private compute/transfer queues fixes this without patching
FFmpeg or enabling CPU pixel transfers. The existence of transfer pools alone
does not mean any pixels are downloaded.

The frame-only mode does not test conversion/display. To exercise synchronized
GPU conversion, RD display, seek, stop and deferred resource release, add
`--playback` to the runner command. Add `--render-thread separate` to test
the dedicated renderer thread, or `--shutdown` to quit with frames in flight.

The playback test requires conversion jobs to actually reach the command
buffer and, except for intentional early shutdown, to all retire after their
GPU fences. Neither mode downloads image pixels or verifies a golden image.
Validation does not prove pixel/color correctness.
