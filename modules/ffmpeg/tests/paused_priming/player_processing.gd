extends SceneTree

class TestPlayback extends VideoStreamPlayback:
	var playing := false
	var paused := false
	var updates := 0
	var starts := 0
	var last_delta := -1.0
	var end_on_update := false

	func _play() -> void:
		playing = true
		starts += 1

	func _stop() -> void:
		playing = false

	func _is_playing() -> bool:
		return playing

	func _set_paused(value: bool) -> void:
		paused = value

	func _is_paused() -> bool:
		return paused

	func _update(delta: float) -> void:
		updates += 1
		last_delta = delta
		if end_on_update:
			playing = false

	func _get_texture() -> Texture2D:
		return null

	func _get_channels() -> int:
		return 0

	func _get_mix_rate() -> int:
		return 48000

class TestStream extends VideoStream:
	var playback := TestPlayback.new()

	func _instantiate_playback() -> VideoStreamPlayback:
		return playback

var failed := false
var finished_count := 0

func _initialize() -> void:
	call_deferred("_run")

func _check(condition: bool, message: String) -> void:
	if not condition:
		printerr("PLAYER_PROCESSING_TEST result=FAIL ", message)
		failed = true

func _frames() -> void:
	for index in range(3):
		await process_frame

func _run() -> void:
	var player := VideoStreamPlayer.new()
	var stream := TestStream.new()
	player.stream = stream
	root.add_child(player)
	player.finished.connect(func() -> void: finished_count += 1)
	var playback := stream.playback
	_check(not player.process_while_paused, "property should default to false")
	player.paused = true
	player.play()
	await _frames()
	_check(playback.updates == 0, "default paused player received updates")

	player.process_while_paused = true
	await _frames()
	_check(playback.updates > 0 and playback.last_delta == 0.0, "paused updates did not use zero delta")
	player.process_while_paused = false
	var count := playback.updates
	await _frames()
	_check(playback.updates == count, "disabling property did not stop paused updates")
	player.process_while_paused = true
	await _frames()
	_check(playback.updates > count, "enabling property did not restart paused updates")

	paused = true
	count = playback.updates
	await _frames()
	_check(playback.updates == count, "property bypassed tree pause")
	player.process_mode = Node.PROCESS_MODE_ALWAYS
	await _frames()
	_check(playback.updates > count, "always-processing node did not update in paused tree")
	paused = false
	player.process_mode = Node.PROCESS_MODE_INHERIT
	player.paused = false
	player.process_while_paused = false
	await _frames()
	_check(playback.last_delta > 0.0, "unpaused playback did not receive frame delta")
	player.process_while_paused = true
	player.paused = true
	player.loop = true
	var starts := playback.starts
	playback.end_on_update = true
	await _frames()
	_check(not playback.playing and playback.starts == starts, "paused update looped playback")
	_check(finished_count == 0, "paused update emitted finished")

	player.stop()
	player.process_while_paused = false
	player.process_while_paused = true
	count = playback.updates
	await _frames()
	_check(playback.updates == count, "property restarted stopped playback")
	player.queue_free()
	await process_frame
	print("PLAYER_PROCESSING_TEST result=", "FAIL" if failed else "PASS")
	quit(1 if failed else 0)
