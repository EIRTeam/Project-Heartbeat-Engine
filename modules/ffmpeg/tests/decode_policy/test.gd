extends SceneTree

var failed := false

func _initialize() -> void:
	call_deferred("_run")

func _check(condition: bool, message: String) -> void:
	if not condition:
		printerr("DECODE_POLICY_TEST result=FAIL ", message)
		failed = true

func _music_time() -> float:
	return 1.213

func _player(path: String, policy: int) -> VideoStreamPlayer:
	var stream := FFmpegVideoStream.new()
	stream.file = path
	stream.decode_policy = policy
	var player := VideoStreamPlayer.new()
	player.stream = stream
	root.add_child(player)
	return player

func _wait_backend(player: VideoStreamPlayer, backend: String) -> bool:
	var playback := player.get_stream_playback()
	if playback == null:
		_check(false, "playback creation failed")
		return false
	var deadline := Time.get_ticks_msec() + 4000
	while Time.get_ticks_msec() < deadline:
		await process_frame
		if playback.call("get_decode_backend") == backend and playback.call("get_playback_timing").has_frame \
				and player.get_video_texture() != null and player.get_video_texture().get_width() > 0:
			_check(bool(playback.call("is_using_gpu_decoding")) == (backend == "vulkan"), "backend/GPU boolean disagree")
			return true
	_check(false, "did not produce " + backend + " frames")
	return false

func _dispose(player: VideoStreamPlayer) -> void:
	player.stop()
	player.queue_free()
	await create_timer(0.15).timeout

func _watch_failures(player: VideoStreamPlayer) -> Dictionary:
	var events := {"count": 0, "reason": "", "finished": 0, "main_thread": true}
	player.get_stream_playback().connect("gpu_playback_failed", func(reason: String) -> void:
		events.count += 1
		events.reason = reason
		events.main_thread = OS.get_thread_caller_id() == OS.get_main_thread_id()
	)
	player.finished.connect(func() -> void: events.finished += 1)
	return events

func _check_failure(player: VideoStreamPlayer, events: Dictionary, terminal: bool) -> void:
	# Signals are deferred, including failures detected during construction.
	await process_frame
	await process_frame
	var playback := player.get_stream_playback()
	_check(playback.call("has_gpu_playback_failed"), "GPU failure state was not retained")
	_check(not str(playback.call("get_gpu_failure_reason")).is_empty(), "missing GPU failure reason")
	_check(events.count == 1, "GPU failure signal was missing or duplicated")
	_check(events.reason == playback.call("get_gpu_failure_reason"), "signal/query failure reasons differ")
	_check(events.main_thread, "GPU failure signal ran off the main thread")
	_check(bool(playback.call("has_playback_failed")) == terminal, "terminal failure state disagrees with fallback")
	_check(events.finished == 0, "GPU failure emitted finished")

func _check_inert(player: VideoStreamPlayer, events: Dictionary) -> void:
	player.loop = true
	player.play()
	player.stream_position = 0.5
	player.stop()
	player.play()
	await _check_failure(player, events, true)
	_check(not player.is_playing(), "failed playback restarted")
	_check(player.get_video_texture() == null, "failed playback retained video output")
	var timing: Dictionary = player.get_stream_playback().call("get_playback_timing")
	_check(not timing.has_frame and timing.queued_frames == 0, "failed playback retained decoded frames")

func _run() -> void:
	var args := OS.get_cmdline_user_args()
	if args.size() != 3:
		quit(2)
		return
	var h264: String = args[0]
	var unsupported: String = args[1]
	var scenario: String = args[2]
	var policy_setting := "multimedia/ffmpeg/default_decode_policy"
	_check(ProjectSettings.has_setting(policy_setting), "project decode policy is not registered")
	_check(int(ProjectSettings.get_setting(policy_setting)) == FFmpegVideoStream.DECODE_DEFAULT, "project default does not preserve existing behavior")
	# Runtime changes affect new playback, not already instantiated playback.
	ProjectSettings.set_setting(policy_setting, FFmpegVideoStream.DECODE_SOFTWARE_ONLY)
	var default_player := _player(h264, FFmpegVideoStream.DECODE_DEFAULT)
	default_player.play()
	await _wait_backend(default_player, "software")
	_check(not default_player.get_stream_playback().call("has_gpu_playback_failed"), "software-only reported a GPU failure")
	ProjectSettings.set_setting(policy_setting, FFmpegVideoStream.DECODE_REQUIRE_GPU)
	default_player.stop()
	default_player.play()
	await _wait_backend(default_player, "software")
	await _dispose(default_player)
	# An explicit software policy overrides a strict GPU project default.
	default_player = _player(h264, FFmpegVideoStream.DECODE_SOFTWARE_ONLY)
	default_player.play()
	await _wait_backend(default_player, "software")
	await _dispose(default_player)
	# Newly created default playback picks up Prefer GPU, including fallback.
	ProjectSettings.set_setting(policy_setting, FFmpegVideoStream.DECODE_PREFER_GPU)
	default_player = _player(unsupported, FFmpegVideoStream.DECODE_DEFAULT)
	var default_events := _watch_failures(default_player)
	_check(default_events.count == 0, "initialization failure signal was not deferred")
	default_player.play()
	await _wait_backend(default_player, "software")
	await _check_failure(default_player, default_events, false)
	_check(not str(default_player.get_stream_playback().call("get_decode_fallback_reason")).is_empty(), "project Prefer GPU did not report fallback")
	await _dispose(default_player)
	ProjectSettings.set_setting(policy_setting, FFmpegVideoStream.DECODE_DEFAULT)
	if scenario.begins_with("strict"):
		for policies in [
			[FFmpegVideoStream.DECODE_REQUIRE_GPU, FFmpegVideoStream.DECODE_DEFAULT],
			[FFmpegVideoStream.DECODE_DEFAULT, FFmpegVideoStream.DECODE_DEFAULT],
			[FFmpegVideoStream.DECODE_DEFAULT, FFmpegVideoStream.DECODE_REQUIRE_GPU],
		]:
			ProjectSettings.set_setting(policy_setting, policies[1])
			var player := _player(unsupported, policies[0])
			_check(player.get_stream_playback() != null, "GPU initialization failure discarded playback")
			if player.get_stream_playback() != null:
				var events := _watch_failures(player)
				_check(player.get_stream_playback().call("has_gpu_playback_failed"), "initialization failure was not immediately queryable")
				await _check_inert(player, events)
			await _dispose(player)
	elif scenario == "software_device" or scenario == "compiled_out":
		var player := _player(h264, FFmpegVideoStream.DECODE_PREFER_GPU)
		var events := _watch_failures(player)
		player.play()
		await _wait_backend(player, "software")
		await _check_failure(player, events, false)
		_check(not str(player.get_stream_playback().call("get_decode_fallback_reason")).is_empty(), "missing unavailable-device fallback reason")
		# Software RGBA output must survive stop/play, too.
		player.stop()
		player.play()
		await _wait_backend(player, "software")
		await _dispose(player)
	else:
		var player := _player(h264, FFmpegVideoStream.DECODE_SOFTWARE_ONLY)
		player.play()
		await _wait_backend(player, "software")
		_check(player.get_stream_playback().call("get_decode_fallback_reason") == "", "software-only is not a fallback")
		await _dispose(player)

		player = _player(unsupported, FFmpegVideoStream.DECODE_PREFER_GPU)
		player.play()
		await _wait_backend(player, "software")
		_check(not str(player.get_stream_playback().call("get_decode_fallback_reason")).is_empty(), "missing unsupported-codec fallback reason")
		await _dispose(player)

		# Project-default stays software with the output opt-in off.
		ProjectSettings.set_setting("rendering/rendering_device/vulkan/video_decode_output", false)
		player = _player(h264, FFmpegVideoStream.DECODE_DEFAULT)
		player.play()
		await _wait_backend(player, "software")
		await _dispose(player)
		# An explicit policy overrides a software project default and opts into
		# output even without the global output flag.
		ProjectSettings.set_setting(policy_setting, FFmpegVideoStream.DECODE_SOFTWARE_ONLY)
		player = _player(h264, FFmpegVideoStream.DECODE_PREFER_GPU)
		var playback := player.get_stream_playback()
		var events := _watch_failures(player)
		playback.call("set_playback_clock", _music_time)
		player.play()
		if await _wait_backend(player, "vulkan"):
			_check(playback.call("get_decode_fallback_reason") == "", "successful GPU decode reported fallback")
			_check(not playback.call("has_gpu_playback_failed") and events.count == 0, "successful GPU playback reported failure")
			if playback.has_method("test_force_gpu_failure"):
				var old_texture := player.get_video_texture()
				playback.call("test_force_gpu_failure")
				await _wait_backend(player, "software")
				await _check_failure(player, events, false)
				var timing: Dictionary = playback.call("get_playback_timing")
				_check(player.is_playing(), "runtime fallback stopped playback")
				_check(abs(float(timing.clock_time) - 1.213) < 0.001, "fallback reset the music clock")
				_check(abs(float(timing.selected_frame_time) - 1.2) < 0.035, "fallback did not seek to the music position")
				_check(player.get_video_texture() != old_texture, "VideoStreamPlayer kept the old GPU texture")
				_check(not str(playback.call("get_decode_fallback_reason")).is_empty(), "runtime fallback reason missing")
			else:
				_check(false, "runtime fallback test requires ffmpeg_gpu_tests=yes")
		await _dispose(player)
		# A project GPU policy also opts into output. Changing the default while
		# it is active must not disable its runtime software recovery.
		ProjectSettings.set_setting(policy_setting, FFmpegVideoStream.DECODE_PREFER_GPU)
		player = _player(h264, FFmpegVideoStream.DECODE_DEFAULT)
		player.play()
		if await _wait_backend(player, "vulkan"):
			ProjectSettings.set_setting(policy_setting, FFmpegVideoStream.DECODE_REQUIRE_GPU)
			player.get_stream_playback().call("test_force_gpu_failure")
			await _wait_backend(player, "software")
			_check(not str(player.get_stream_playback().call("get_decode_fallback_reason")).is_empty(), "project policy runtime fallback reason missing")
		await _dispose(player)
		ProjectSettings.set_setting(policy_setting, FFmpegVideoStream.DECODE_DEFAULT)
		# Standalone playback also hands embedded audio to a fresh resampler
		# and continues its timeline instead of restarting from zero.
		player = _player(h264, FFmpegVideoStream.DECODE_PREFER_GPU)
		player.play()
		if await _wait_backend(player, "vulkan"):
			await create_timer(0.2).timeout
			var before := player.stream_position
			player.get_stream_playback().call("test_force_gpu_failure")
			await _wait_backend(player, "software")
			_check(player.is_playing() and player.stream_position >= before, "standalone fallback reset playback")
		await _dispose(player)
		# A strict runtime failure is inert, emits once, and never finishes/loops.
		player = _player(h264, FFmpegVideoStream.DECODE_REQUIRE_GPU)
		events = _watch_failures(player)
		player.loop = true
		player.play()
		if await _wait_backend(player, "vulkan"):
			player.get_stream_playback().call("test_force_gpu_failure")
			var deadline := Time.get_ticks_msec() + 4000
			while player.is_playing() and Time.get_ticks_msec() < deadline:
				await process_frame
			await _check_inert(player, events)
		await _dispose(player)
		ProjectSettings.set_setting("rendering/rendering_device/vulkan/video_decode_output", true)
	await create_timer(0.2).timeout
	print("DECODE_POLICY_TEST scenario=", scenario, " result=", "FAIL" if failed else "PASS")
	quit(1 if failed else 0)
