extends SceneTree

class MusicClock extends RefCounted:
	var position := 0.0
	var rate := 0.0
	var anchor_usec := Time.get_ticks_usec()

	func read() -> float:
		return position + (Time.get_ticks_usec() - anchor_usec) / 1_000_000.0 * rate

	func reset(time: float, speed: float = 0.0) -> void:
		position = time
		rate = speed
		anchor_usec = Time.get_ticks_usec()

var player: VideoStreamPlayer
var clock := MusicClock.new()
var playback: VideoStreamPlayback
var failed := false

func _initialize() -> void:
	call_deferred("_run")

func _check(condition: bool, message: String) -> void:
	if not condition:
		printerr("VIDEO_CLOCK_TEST result=FAIL ", message)
		failed = true

func _settle(target: float, name: String, max_lateness: float = 0.035) -> void:
	clock.reset(target)
	var deadline := Time.get_ticks_msec() + 4000
	var synced := false
	while Time.get_ticks_msec() < deadline:
		await process_frame
		var timing: Dictionary = playback.call("get_playback_timing")
		var lateness := float(timing.selected_frame_lateness)
		if timing.has_frame and abs(float(timing.clock_time) - target) < 0.0001 and lateness >= -0.0001 and lateness < max_lateness:
			synced = true
			break
	_check(synced, name + ": did not select the frame covering a frozen music clock")
	# Holding a clock must not advance video or continuously seek.
	var before: Dictionary = playback.call("get_playback_timing")
	await create_timer(0.15).timeout
	var after: Dictionary = playback.call("get_playback_timing")
	_check(before.selected_frame_time == after.selected_frame_time, name + ": video advanced while music paused")
	_check(before.clock_seeks == after.clock_seeks, name + ": seek loop while music paused")
	_check(int(after.queued_frames) < 10, name + ": unbounded paused queue")
	print("VIDEO_CLOCK_PHASE name=", name, " selected=", after.selected_frame_time, " target=", target, " result=", "FAIL" if failed else "PASS")

func _run_rate(speed: float, name: String) -> void:
	clock.reset(clock.read(), speed)
	var deadline := Time.get_ticks_msec() + 600
	var max_lateness := 0.0
	var seeks_before: int = playback.call("get_playback_timing").clock_seeks
	while Time.get_ticks_msec() < deadline:
		await process_frame
		var timing: Dictionary = playback.call("get_playback_timing")
		_check(float(timing.selected_frame_lateness) >= -0.001, name + ": selected a future frame")
		max_lateness = max(max_lateness, float(timing.selected_frame_lateness))
	_check(max_lateness < 0.15, name + ": video fell behind the music")
	_check(int(playback.call("get_playback_timing").clock_seeks) == seeks_before, name + ": ordinary playback caused a seek")
	print("VIDEO_CLOCK_PHASE name=", name, " max_selected_lateness=", max_lateness, " result=", "FAIL" if failed else "PASS")

func _test_hitch_and_jitter() -> void:
	var seeks_before: int = playback.call("get_playback_timing").clock_seeks
	var drops_before: int = playback.call("get_playback_timing").dropped_frames
	var held_position: float = playback.call("get_playback_timing").clock_time
	clock.reset(held_position - 0.02)
	await create_timer(0.15).timeout
	_check(int(playback.call("get_playback_timing").clock_seeks) == seeks_before, "small music-clock correction caused a seek")
	clock.reset(held_position, 1.0)
	# Audio time keeps advancing while the game's frame loop is stalled.
	OS.delay_msec(700)
	await create_timer(0.4).timeout
	var timing: Dictionary = playback.call("get_playback_timing")
	_check(player.is_playing(), "playback stopped after a game-thread hitch")
	_check(int(timing.clock_seeks) == seeks_before, "game-thread hitch caused an unnecessary recovery seek")
	_check(float(timing.selected_frame_lateness) < 0.15, "video did not catch up by dropping old frames")
	_check(int(timing.dropped_frames) > drops_before, "hitch recovery did not discard obsolete frames")
	_check(timing.stop_reason == "", "unexpected fault during catch-up")
	print("VIDEO_CLOCK_PHASE name=hitch_and_jitter lateness=", timing.selected_frame_lateness, " result=", "FAIL" if failed else "PASS")

func _run() -> void:
	var args := OS.get_cmdline_user_args()
	if args.is_empty():
		quit(2)
		return
	if "software" in args:
		ProjectSettings.set_setting("rendering/rendering_device/vulkan/video_decode_output", false)
	var stream := FFmpegVideoStream.new()
	stream.file = args[0]
	player = VideoStreamPlayer.new()
	player.stream = stream
	player.expand = true
	player.size = Vector2(320, 240)
	root.add_child(player)
	playback = player.get_stream_playback()
	if not playback.has_method("set_playback_clock") or player.get_stream_length() < 6.0:
		printerr("Expected clock-enabled FFmpeg and a >=6-second 30fps video")
		quit(2)
		return
	playback.call("set_playback_clock", clock.read)
	var other_clock := MusicClock.new()
	other_clock.reset(0.813)
	var other_player := VideoStreamPlayer.new()
	other_player.stream = stream
	other_player.expand = true
	other_player.position = Vector2(320, 0)
	other_player.size = Vector2(320, 240)
	root.add_child(other_player)
	var other_playback := other_player.get_stream_playback()
	other_playback.call("set_playback_clock", other_clock.read)
	other_player.play()
	clock.reset(-0.2)
	player.play()
	# This source can run fast, slowly, pause, jump, or wrap independently of
	# engine delta/speed_scale. Non-frame-aligned times also test seek preroll.
	await create_timer(0.2).timeout
	_check(not playback.call("get_playback_timing").has_frame, "video displayed before the song started")
	await _settle(0.213, "start")
	_check(is_equal_approx(float(playback.call("get_clock_recovery_tolerance")), 1.5), "unexpected recovery tolerance")
	playback.call("set_clock_recovery_tolerance", 2.0)
	_check(is_equal_approx(float(playback.call("get_clock_recovery_tolerance")), 2.0), "could not configure recovery tolerance")
	playback.call("set_clock_recovery_tolerance", 1.5)
	await _test_hitch_and_jitter()
	await _run_rate(1.0, "normal")
	player.speed_scale = 0.25
	await _run_rate(1.75, "music_rate")
	await _settle(4.213, "forward_seek")
	await _settle(0.413, "backward_seek")
	await _run_rate(0.5, "slow_music")
	await _settle(player.get_stream_length() + 0.2, "past_end", 0.3)
	await _settle(0.213, "loop_after_end")
	var other_timing: Dictionary = other_playback.call("get_playback_timing")
	_check(abs(float(other_timing.selected_frame_time) - 0.8) < 0.001, "players shared their music clocks")
	print("VIDEO_CLOCK_PHASE name=independent_players result=", "FAIL" if failed else "PASS")
	other_player.stop()
	other_player.queue_free()
	player.stop()
	playback.call("set_playback_clock", Callable())
	_check(not playback.call("get_playback_timing").external_clock, "could not restore standalone mode")
	player.speed_scale = 1.0
	player.play()
	await create_timer(0.3).timeout
	_check(float(playback.call("get_playback_timing").clock_time) > 0.1, "standalone clock did not resume")
	print("VIDEO_CLOCK_PHASE name=standalone result=", "FAIL" if failed else "PASS")
	player.stop()
	player.queue_free()
	await create_timer(0.2).timeout
	print("VIDEO_CLOCK_TEST result=", "FAIL" if failed else "PASS")
	quit(1 if failed else 0)
