extends Compositor2D

var calls := 0
var saw_screen := false
var saw_backbuffer := false

func _render_callback(data: RenderCanvasDataRD) -> void:
	calls += 1
	saw_screen = data.get_screen_texture().is_valid()
	saw_backbuffer = data.get_backbuffer_texture().is_valid()
