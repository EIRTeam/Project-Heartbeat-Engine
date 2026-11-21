extends SceneTree

func _initialize() -> void:
	call_deferred("_run")

func _stats(texture: RID = RID()) -> Dictionary:
	return ClassDB.class_call_static("FFmpegVideoStream", "test_vulkan_output_stats", texture)

func _player_stats(player: VideoStreamPlayer) -> Dictionary:
	var texture := player.get_video_texture() as Texture2DRD
	if texture == null or texture.get_width() <= 0 or texture.get_height() <= 0:
		return {}
	return _stats(texture.get_texture_rd_rid())

func _run() -> void:
	var args := OS.get_cmdline_user_args()
	if args.size() < 1 or not ClassDB.class_has_method("FFmpegVideoStream", "test_vulkan_output_stats"):
		printerr("Expected a video path and an ffmpeg_gpu_tests=yes build")
		quit(2)
		return
	var color_test := "color-bars" in args
	if color_test:
		ClassDB.class_call_static("FFmpegVideoStream", "test_vulkan_color_validation")
	if not _stats().get("dpb_gate_ok", false):
		printerr("Distinct-DPB safety gate self-test failed")
		quit(1)
		return
	var players: Array[VideoStreamPlayer] = []
	for index in range(2):
		var video := FFmpegVideoStream.new()
		video.file = args[0]
		var player := VideoStreamPlayer.new()
		player.stream = video
		player.expand = true
		player.size = Vector2(320, 240)
		player.position = Vector2(index * 320, 0)
		root.add_child(player)
		if index == 0 or not "skip-second" in args:
			player.play()
		players.push_back(player)
	await create_timer(0.6).timeout
	var initial: Array[Dictionary] = []
	for index in range(players.size()):
		var snapshot := _player_stats(players[index])
		if snapshot.get("submitted", 0) < 2 or snapshot.get("completed", 0) < 1 or players[index].stream_position <= 0.1:
			printerr("Player ", index, " did not present progressing GPU frames")
			quit(1)
			return
		initial.push_back(snapshot)
	if "shutdown" in args:
		print("VULKAN_PLAYBACK_SHUTDOWN submitted=", _stats().submitted)
		quit(0)
		return
	await create_timer(0.3).timeout
	for index in range(players.size()):
		var snapshot := _player_stats(players[index])
		if snapshot.get("last_pts", 0) <= initial[index].get("last_pts", 0):
			printerr("Player ", index, " decoded presentation timestamps did not advance")
			quit(1)
			return
		initial[index] = snapshot
		players[index].stream_position = 0.0
	await create_timer(0.3).timeout
	for index in range(players.size()):
		var snapshot := _player_stats(players[index])
		if snapshot.get("submitted", 0) <= initial[index].submitted or snapshot.get("last_pts", 0) >= initial[index].last_pts:
			printerr("Player ", index, " did not present new GPU frames after seeking")
			quit(1)
			return
		if color_test and (snapshot.get("color_checked", 0) < 8 or snapshot.get("color_failed", 0) != 0):
			printerr("Player ", index, " failed GPU reference color checks: ", snapshot)
			quit(1)
			return
		print("VULKAN_PLAYBACK_PLAYER index=", index, " submitted=", snapshot.submitted, " completed=", snapshot.completed, " pts=", snapshot.last_pts, " result=PASS")
		players[index].stop()
		players[index].queue_free()
	players.clear()
	await create_timer(1.0).timeout
	var final_stats := _stats()
	print("VULKAN_PLAYBACK_COLOR checked=", final_stats.color_checked, " failed=", final_stats.color_failed)
	print("VULKAN_PLAYBACK_TEST submitted=", final_stats.submitted, " completed=", final_stats.completed)
	quit(0 if final_stats.completed == final_stats.submitted and (not color_test or final_stats.color_failed == 0) else 1)
