#include "ph_blur_controls.h"
#include "ph_singleton.h"
#include "scene/resources/style_box_flat.h"

Ref<ShaderMaterial> HBStyleboxBlurDrawer::blur_material;

void HBButtonBlurEX::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_DRAW: {
			Ref<StyleBox> stylebox = _get_current_stylebox();
			blur_drawer->set_stylebox_original(stylebox);
		} break;
	}
}

HBButtonBlurEX::HBButtonBlurEX() {
	blur_drawer = memnew(HBStyleboxBlurDrawer);
	add_child(blur_drawer);
}

void HBStyleboxBlurDrawer::_update_stylebox() {
	if (!stylebox_original.is_valid()) {
		stylebox.unref();
	}
	stylebox = stylebox_original->duplicate();
	Ref<StyleBoxFlat> flat_sb = stylebox;
	if (flat_sb.is_valid()) {
		// Get rid of things such as shadows and simplify it, this way the mesh
		// will only be what we need for blur
		flat_sb->set_shadow_size(0);
		flat_sb->set_bg_color(Color(1.0f, 1.0, 1.0f, 1.0f));
		flat_sb->set_border_color(Color(1.0f, 1.0, 1.0f, 1.0f));
		flat_sb->set_border_width_all(0);
		flat_sb->set_anti_aliased(false);
	}

	if (stylebox_original.is_valid() && PHNative::get_singleton()->get_blur_controls_enabled()) {
		show();
	} else {
		hide();
	}
}

void HBStyleboxBlurDrawer::_on_blur_controls_enabled_changed() {
	if (PHNative::get_singleton()->get_blur_controls_enabled()) {
		show();
		queue_redraw();
	} else {
		hide();
	}
}

void HBStyleboxBlurDrawer::set_stylebox_original(Ref<StyleBox> p_stylebox) {
	if (stylebox_original.is_valid()) {
		stylebox_original->disconnect(SNAME("changed"), callable_mp(this, &HBStyleboxBlurDrawer::_update_stylebox));
	}

	Ref<StyleBox> old_original = stylebox_original;
	stylebox_original = p_stylebox;

	stylebox_original->connect(SNAME("changed"), callable_mp(this, &HBStyleboxBlurDrawer::_update_stylebox));

	if (old_original != stylebox_original) {
		_update_stylebox();
	}

	queue_redraw();
}

void HBStyleboxBlurDrawer::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			PHNative::get_singleton()->connect(SNAME("blur_controls_enabled_changed"), callable_mp(this, &HBStyleboxBlurDrawer::_on_blur_controls_enabled_changed));
			_on_blur_controls_enabled_changed();
		} break;
		case NOTIFICATION_EXIT_TREE: {
			PHNative::get_singleton()->disconnect(SNAME("blur_controls_enabled_changed"), callable_mp(this, &HBStyleboxBlurDrawer::_on_blur_controls_enabled_changed));
		} break;
		case NOTIFICATION_DRAW: {
			const RID ci = get_canvas_item();
			if (!RenderingDevice::get_singleton()) {
				return;
			}
			if (stylebox.is_valid()) {
				stylebox->draw(ci, Rect2(Vector2(0.0f, 0.0f), get_size()));
			}
		} break;
	}
}

HBStyleboxBlurDrawer::HBStyleboxBlurDrawer() {
	set_material(blur_material);
	set_draw_behind_parent(true);
	set_anchors_and_offsets_preset(LayoutPreset::PRESET_FULL_RECT);
	set_mouse_filter(MOUSE_FILTER_IGNORE);
}

void HBPanelContainerBlurEX::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_DRAW: {
			blur_drawer->set_stylebox_original(get_theme_stylebox(SNAME("panel")));
		} break;
	}
}

HBPanelContainerBlurEX::HBPanelContainerBlurEX() {
	blur_drawer = memnew(HBStyleboxBlurDrawer);
	add_child(blur_drawer);
}

void HBPanelBlurEX::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_DRAW: {
			blur_drawer->set_stylebox_original(get_theme_stylebox(SNAME("panel")));
		} break;
	}
}

HBPanelBlurEX::HBPanelBlurEX() {
	blur_drawer = memnew(HBStyleboxBlurDrawer);
	add_child(blur_drawer);
}
