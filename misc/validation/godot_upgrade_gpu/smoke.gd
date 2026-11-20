extends SceneTree

var failures := 0

func check(condition: bool, description: String) -> void:
	if not condition:
		push_error(description)
		failures += 1

func _initialize() -> void:
	call_deferred("_run")

func _run() -> void:
	print("GPU_ADAPTER: ", RenderingServer.get_video_adapter_name())
	var viewport := SubViewport.new()
	viewport.size = Vector2i(64, 64)
	viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(viewport)
	var background := ColorRect.new()
	background.size = Vector2(64, 64)
	background.color = Color.RED
	viewport.add_child(background)
	var clip := Control.new()
	clip.position = Vector2(8, 8)
	clip.size = Vector2(16, 16)
	clip.clip_contents = true
	viewport.add_child(clip)
	var fill := ColorRect.new()
	fill.size = Vector2(64, 64)
	fill.color = Color.BLUE
	clip.add_child(fill)
	for frame in range(3):
		await RenderingServer.frame_post_draw
	var image := viewport.get_texture().get_image()
	check(image != null and not image.is_empty(), "Rendered image must be available")
	if image != null and not image.is_empty():
		check(image.get_pixel(12, 12).b > 0.9, "Clip interior must be blue")
		check(image.get_pixel(40, 40).r > 0.9, "Clip exterior must remain red")
		image.save_png("user://clipping.png")

	if RenderingServer.get_rendering_device() != null:
		var compositor = load("res://compositor_probe.gd").new()
		compositor.enable_backbuffer()
		viewport.add_child(compositor)
		for frame in range(3):
			await RenderingServer.frame_post_draw
		check(compositor.calls > 0, "Compositor2D callback must execute")
		check(compositor.saw_screen, "Compositor2D screen texture must be valid")
		check(compositor.saw_backbuffer, "Compositor2D backbuffer texture must be valid")
		compositor.queue_free()
		await process_frame

	for iteration in range(3):
		var stream := FFmpegVideoStream.new()
		stream.file = "res://red.mp4"
		var player := VideoStreamPlayer.new()
		player.stream = stream
		root.add_child(player)
		player.play()
		await create_timer(0.4).timeout
		check(player.is_playing(), "Video playback must start")
		var texture := player.get_video_texture()
		check(texture != null and texture.get_size() == Vector2(160, 96), "Video texture size")
		if texture != null and texture.get_size() == Vector2(160, 96):
			# Read the rendered result, not a GPU texture without a readback usage flag.
			viewport.size = Vector2i(160, 96)
			var presentation := TextureRect.new()
			presentation.texture = texture
			presentation.size = Vector2(160, 96)
			viewport.add_child(presentation)
			for frame in range(2):
				await RenderingServer.frame_post_draw
			var video_image := viewport.get_texture().get_image()
			check(video_image != null and not video_image.is_empty(), "Decoded frame must be readable")
			if video_image != null and not video_image.is_empty():
				var pixel := video_image.get_pixel(80, 48)
				check(pixel.r > 0.8 and pixel.g < 0.2 and pixel.b < 0.2, "Decoded red frame color")
			presentation.queue_free()
			await process_frame
		player.stream_position = 0.8
		await create_timer(0.2).timeout
		check(player.stream_position >= 0.8, "Video seek must advance")
		player.stop()
		player.queue_free()
		await process_frame
		await RenderingServer.frame_post_draw
		check(is_instance_valid(texture), "Texture wrapper may outlive player teardown")
	viewport.queue_free()
	await process_frame
	await RenderingServer.frame_post_draw
	if failures == 0:
		print("PH_GPU_MEDIA_PASS")
	quit(0 if failures == 0 else 1)
