/*************************************************************************/
/*  audio_stream_editor_plugin.cpp                                       */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2021 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2021 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "ph_audio_stream_editor.h"

#include "core/config/project_settings.h"
#include "core/io/resource_loader.h"
#include "ph_audio_stream_preview.h"

void PHAudioStreamEditor::_notification(int p_what) {
	if (p_what == NOTIFICATION_THEME_CHANGED || p_what == NOTIFICATION_ENTER_TREE) {
		_preview->set_color(Color(0.0, 0.0, 0.0, 0.0));
		set_color(get_color());

		_preview->queue_redraw();
	}
}

void PHAudioStreamEditor::_draw_preview() {
	if (!stream.is_valid()) {
		return;
	}

	Rect2 rect = _preview->get_rect();
	Size2 size = get_size();

	Ref<PHAudioStreamPreview> preview = PHAudioStreamPreviewGenerator::get_singleton()->generate_preview(stream);
	float preview_len = preview->get_length();

	float start_point_c = MIN(start_point, preview_len);
	float end_point_c = end_point;

	ERR_FAIL_COND_MSG(end_point_c <= start_point_c, "End point must be after the start point");

	Vector<Vector2> lines;
	lines.resize(size.width * 2);

	float len = end_point_c - start_point_c;

	for (int i = 0; i < size.width; i++) {
		float ofs = start_point_c + (i * len / size.width);
		float ofs_n = start_point_c + ((i + 1) * len / size.width);
		float avg = preview->get_avg(ofs, ofs_n);
		float max = avg * 0.5 + 0.5;
		float min = -avg * 0.5 + 0.5;

		int idx = i;
		lines.write[idx * 2 + 0] = Vector2(i + 1, rect.position.y + min * rect.size.y);
		lines.write[idx * 2 + 1] = Vector2(i + 1, rect.position.y + max * rect.size.y);
	}

	Vector<Color> colors;
	colors.push_back(get_theme_color("contrast_color_1", "Editor"));

	RS::get_singleton()->canvas_item_add_multiline(_preview->get_canvas_item(), lines, colors);

	if (show_rms) {
		Vector<Vector2> lines_rms;
		lines_rms.resize(size.width * 2);

		for (int i = 0; i < size.width; i++) {
			float ofs = start_point_c + (i * len / size.width);
			float ofs_n = start_point_c + ((i + 1) * len / size.width);
			float avg = preview->get_rms(ofs, ofs_n) * rms_size_multiplier;
			float max = avg * 0.5 + 0.5;
			float min = -avg * 0.5 + 0.5;

			int idx = i;
			lines_rms.write[idx * 2 + 0] = Vector2(i + 1, rect.position.y + min * rect.size.y);
			lines_rms.write[idx * 2 + 1] = Vector2(i + 1, rect.position.y + max * rect.size.y);
		}

		Vector<Color> color_rms;
		color_rms.push_back(get_theme_color("contrast_color_2", "Editor").lightened(0.25));
		RS::get_singleton()->canvas_item_add_multiline(_preview->get_canvas_item(), lines_rms, color_rms);
	}
}

void PHAudioStreamEditor::_preview_changed(ObjectID p_which) {
	if (stream.is_valid() && stream->get_instance_id() == p_which) {
		_preview->queue_redraw();
	}
}

void PHAudioStreamEditor::_changed_callback(Object *p_changed, const char *p_prop) {
	if (!is_visible())
		return;
	queue_redraw();
}

void PHAudioStreamEditor::edit(Ref<AudioStream> p_stream) {
	if (!stream.is_null())
		stream->disconnect("changed", callable_mp((CanvasItem *)this, &CanvasItem::queue_redraw));

	stream = p_stream;

	if (!stream.is_null()) {
		stream->connect("changed", callable_mp((CanvasItem *)this, &CanvasItem::queue_redraw));
		queue_redraw();
	} else {
		hide();
	}
}

void PHAudioStreamEditor::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_preview_changed"), &PHAudioStreamEditor::_preview_changed);
	ClassDB::bind_method(D_METHOD("_draw_preview"), &PHAudioStreamEditor::_draw_preview);
	ClassDB::bind_method(D_METHOD("edit", "stream"), &PHAudioStreamEditor::edit);

	ClassDB::bind_method(D_METHOD("set_rms_size_multiplier", "size"), &PHAudioStreamEditor::set_rms_size_multiplier);
	ClassDB::bind_method(D_METHOD("get_rms_size_multiplier"), &PHAudioStreamEditor::get_rms_size_multiplier);

	ClassDB::bind_method(D_METHOD("set_show_rms", "enabled"), &PHAudioStreamEditor::set_show_rms);
	ClassDB::bind_method(D_METHOD("get_show_rms"), &PHAudioStreamEditor::get_show_rms);

	ClassDB::bind_method(D_METHOD("set_start_point", "point"), &PHAudioStreamEditor::set_start_point);
	ClassDB::bind_method(D_METHOD("get_start_point"), &PHAudioStreamEditor::get_start_point);
	ClassDB::bind_method(D_METHOD("set_end_point", "point"), &PHAudioStreamEditor::set_end_point);
	ClassDB::bind_method(D_METHOD("get_end_point"), &PHAudioStreamEditor::get_end_point);

	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "color"), "set_color", "get_color");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_rms"), "set_show_rms", "get_show_rms");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "rms_size_multiplier"), "set_rms_size_multiplier", "get_rms_size_multiplier");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "start_point"), "set_start_point", "get_start_point");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "end_point"), "set_end_point", "get_end_point");
}

float PHAudioStreamEditor::get_rms_size_multiplier() {
	return rms_size_multiplier;
}

void PHAudioStreamEditor::set_rms_size_multiplier(float p_size) {
	rms_size_multiplier = p_size;
	queue_redraw();
}

void PHAudioStreamEditor::set_show_rms(bool p_enabled) {
	show_rms = p_enabled;
	queue_redraw();
}

bool PHAudioStreamEditor::get_show_rms() {
	return show_rms;
}

void PHAudioStreamEditor::set_start_point(float p_start_point) {
	start_point = p_start_point;
	_preview->queue_redraw();
}

float PHAudioStreamEditor::get_start_point() {
	return start_point;
}

void PHAudioStreamEditor::set_end_point(float p_end_point) {
	end_point = p_end_point;
	_preview->queue_redraw();
}

float PHAudioStreamEditor::get_end_point() {
	return end_point;
}

PHAudioStreamEditor::PHAudioStreamEditor() {
	PHAudioStreamPreviewGenerator::get_singleton()->connect("preview_updated", callable_mp(this, &PHAudioStreamEditor::_preview_changed));
	set_custom_minimum_size(Size2(1, 100) * 1.0);

	VBoxContainer *vbox = memnew(VBoxContainer);
	vbox->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	vbox->set_anchors_and_offsets_preset(PRESET_FULL_RECT, PRESET_MODE_MINSIZE, 0);
	add_child(vbox);

	_preview = memnew(ColorRect);
	_preview->set_v_size_flags(SIZE_EXPAND_FILL);
	_preview->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	_preview->connect("draw", callable_mp(this, &PHAudioStreamEditor::_draw_preview));
	vbox->add_child(_preview);
}
