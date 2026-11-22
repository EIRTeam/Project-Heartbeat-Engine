extends SceneTree

func _initialize() -> void:
	call_deferred("_run")

func _run() -> void:
	var args := OS.get_cmdline_user_args()
	if args.size() != 1:
		printerr("Expected one absolute video path after --")
		quit(2)
		return
	if not ClassDB.class_has_method("FFmpegVideoStream", "test_vulkan_decode"):
		printerr("Rebuild with ffmpeg_gpu_decode=yes ffmpeg_gpu_tests=yes")
		quit(2)
		return
	var result: int = ClassDB.class_call_static("FFmpegVideoStream", "test_vulkan_decode", args[0])
	print("VULKAN_DECODE_TEST result=", result)
	quit(0 if result == OK else 1)
