extends SceneTree

# Run with a real RenderingDevice renderer and one or more YUV420P/YUVA420P
# videos. See README.md for the invocation.
func _initialize() -> void:
	call_deferred("_run")


func _run() -> void:
	if not ClassDB.class_exists("FFmpegVideoStream"):
		push_error("This test needs a build with the FFmpeg module enabled.")
		quit(1)
		return
	if RenderingServer.get_rendering_device() == null:
		push_error("This test needs a RenderingDevice renderer, not headless or Compatibility.")
		quit(1)
		return
	var paths := OS.get_cmdline_user_args()
	if paths.is_empty():
		push_error("Pass YUV420P and/or YUVA420P video paths after --.")
		quit(1)
		return

	for path in paths:
		for iteration in range(100):
			var stream: VideoStream = ClassDB.instantiate("FFmpegVideoStream")
			stream.file = path
			var player := VideoStreamPlayer.new()
			root.add_child(player)
			player.stream = stream
			player.play()
			if not player.is_playing():
				push_error("Could not open " + path)
				player.free()
				quit(1)
				return
			for frame in range(8):
				await process_frame
			player.stream_position = player.get_stream_length() * 0.5
			await process_frame
			# Keep the output wrapper alive beyond playback teardown as a consumer
			# would. Do not synchronize here: leave render work pending.
			var output := player.get_video_texture()
			if not output is Texture2DRD:
				push_error("Use a YUV420P or YUVA420P input to exercise the GPU converter.")
				player.free()
				quit(1)
				return
			if iteration % 3 != 0:
				player.stop()
			# Occasionally synchronize to verify clear ordering; most iterations
			# deliberately release without waiting for render work.
			if iteration % 10 == 1 and iteration % 3 != 0:
				await RenderingServer.frame_post_draw
				var image := output.get_image()
				if image == null or image.is_empty():
					push_error("Missing output image after stop.")
					player.free()
					quit(1)
					return
				for pixel in [Vector2i.ZERO, image.get_size() / 2, image.get_size() - Vector2i.ONE]:
					if image.get_pixelv(pixel) != Color(0, 0, 0, 0):
						push_error("Stop did not clear the output.")
						player.free()
						quit(1)
						return
			if iteration % 3 == 2:
				player.play()
				await process_frame
			player.free()
			stream = null
			await process_frame
			output = null
	await RenderingServer.frame_post_draw
	await RenderingServer.frame_post_draw
	print("FFmpeg GPU converter lifetime stress test completed.")
	quit()
