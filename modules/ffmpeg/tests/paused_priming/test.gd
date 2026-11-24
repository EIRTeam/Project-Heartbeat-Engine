extends SceneTree

var failed := false
var clock_reads := 0
var finished_count := 0
var automatic := false

func _initialize() -> void:
	call_deferred("_run")

func _check(condition: bool, message: String) -> void:
	if not condition:
		printerr("PAUSED_PRIMING_TEST result=FAIL ", message)
		failed = true

func _clock() -> float:
	clock_reads += 1
	return 4.0

func _prime(playback: VideoStreamPlayback, target: float) -> void:
	var deadline := Time.get_ticks_msec() + 4000
	var ready := false
	while Time.get_ticks_msec() < deadline:
		# A nonzero delta must also leave paused playback frozen.
		if not automatic:
			playback.update(1.0)
		await process_frame
		var timing: Dictionary = playback.call("get_playback_timing")
		_check(is_equal_approx(float(timing.clock_time), target), "paused timeline advanced")
		if timing.has_frame and float(timing.selected_frame_time) <= target \
				and target - float(timing.selected_frame_time) < 0.034:
			ready = true
			break
	_check(ready, "did not prime the frame covering " + str(target))

func _pixels(player: VideoStreamPlayer) -> PackedByteArray:
	# Conversion is asynchronous. Read the viewport, not the decoder texture:
	# Vulkan output deliberately does not allow CPU readback.
	await RenderingServer.frame_post_draw
	await RenderingServer.frame_post_draw
	var texture: Texture2D = player.get_video_texture()
	if texture == null:
		_check(false, "missing primed texture")
		return PackedByteArray()
	var image: Image = root.get_texture().get_image()
	if image == null or image.is_empty():
		_check(false, "primed texture has no pixels")
		return PackedByteArray()
	image.convert(Image.FORMAT_RGB8)
	var pixels := image.get_data()
	_check(pixels.count(0) < pixels.size(), "primed texture is still black")
	return pixels

func _run() -> void:
	var args := OS.get_cmdline_user_args()
	if args.is_empty():
		printerr("Expected a >=6-second, 30fps testsrc2 video with audio")
		quit(2)
		return
	var stream := FFmpegVideoStream.new()
	automatic = "automatic" in args
	stream.file = args[0]
	stream.decode_policy = FFmpegVideoStream.DECODE_REQUIRE_GPU if "gpu" in args else FFmpegVideoStream.DECODE_SOFTWARE_ONLY
	var player := VideoStreamPlayer.new()
	if automatic:
		player.process_while_paused = true
	player.stream = stream
	player.expand = true
	player.size = Vector2(320, 180)
	root.add_child(player)
	player.finished.connect(func() -> void: finished_count += 1)
	var playback := player.get_stream_playback()
	if playback == null:
		printerr("PAUSED_PRIMING_TEST result=FAIL playback creation failed")
		quit(1)
		return
	player.play()
	player.paused = true
	await _prime(playback, 0.0)
	var initial_pixels: PackedByteArray = await _pixels(player)
	_check(not initial_pixels.is_empty(), "initial frame was not visible")

	for target in [1.213, 0.413]:
		player.stream_position = target
		await _prime(playback, target)
		var pixels: PackedByteArray = await _pixels(player)
		_check(not pixels.is_empty() and pixels != initial_pixels, "paused seek did not update visible pixels")

	var before: Dictionary = playback.call("get_playback_timing")
	for index in range(30):
		if not automatic:
			playback.update(1.0)
		await process_frame
	var after: Dictionary = playback.call("get_playback_timing")
	_check(before.selected_frame_time == after.selected_frame_time, "held frame advanced")
	_check(int(after.queued_frames) < 10, "unbounded paused queue")
	_check(player.is_playing() and player.paused, "paused state changed")

	# Paused external-clock playback must honor explicit seeks, not sample a
	# moving external clock or recover toward it behind the caller's back.
	player.stop()
	playback.call("set_playback_clock", _clock)
	player.play()
	player.stream_position = 2.213
	await _prime(playback, 2.213)
	_check(clock_reads == 0, "paused update sampled external clock")
	_check(int(playback.call("get_playback_timing").clock_seeks) == 0, "paused clock recovery ran")

	player.stop()
	playback.call("set_playback_clock", Callable())
	player.play()
	player.stream_position = player.get_stream_length() + 0.2
	for index in range(30):
		if not automatic:
			playback.update(1.0)
		await process_frame
	_check(player.is_playing(), "paused EOF ended playback")
	_check(finished_count == 0, "paused EOF emitted finished")
	player.stream_position = 0.213
	await _prime(playback, 0.213)
	player.paused = false
	await create_timer(0.25).timeout
	_check(player.stream_position > 0.3, "resume did not advance playback")
	player.stop()
	playback.update(1.0)
	_check(not player.is_playing(), "stopped update restarted playback")
	_check(not playback.call("get_playback_timing").has_frame, "stopped update primed a frame")
	player.queue_free()
	await process_frame
	print("PAUSED_PRIMING_TEST result=", "FAIL" if failed else "PASS")
	quit(1 if failed else 0)
