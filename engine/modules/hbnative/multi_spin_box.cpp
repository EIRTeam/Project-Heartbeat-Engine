#include "multi_spin_box.h"
#include "core/input/input.h"
#include "core/math/expression.h"
#include "core/os/keyboard.h"

#include "modules/modules_enabled.gen.h" // For csg, gridmap, regex.

#ifdef MODULE_REGEX_ENABLED
#include "modules/regex/regex.h"
#endif // MODULE_REGEX_ENABLED

Size2 MultiSpinBox::get_minimum_size() const {
	Size2 ms = line_edit->get_combined_minimum_size();
	ms.width += last_w;
	return ms;
}

// Get the minimum and maximum properties
double *MultiSpinBox::get_range() const {
	if (inputs.size() == 0)
		return NULL;

	double value = values[0];
	double *range = (double *)malloc(sizeof(double) * 2);
	range[0] = value;
	range[1] = value;

	for (int i = 1; i < inputs.size(); i++) {
		value = values[i];

		range[0] = fmin(range[0], value);
		range[1] = fmax(range[1], value);
	}

	return range;
}

String MultiSpinBox::get_range_string() const {
	double *range = get_range();
	if (range == NULL)
		return String();

	String range_string;

	if (range[0] == range[1]) {
		// Using %.*f with a precision of 0 decimal places seems to silently fail
		if (precision > 0) {
			range_string = vformat("%.*f%s", precision, range[0], suffix);
		} else {
			range_string = vformat("%d%s", range[0], suffix);
		}
	} else {
		// Using %.*f with a precision of 0 decimal places seems to silently fail
		if (precision > 0) {
			// HACK: The maximum amount of params for vformat is 5 ._.
			String min_s = vformat("%.*f%s", precision, range[0], suffix);
			String max_s = vformat("%.*f%s", precision, range[1], suffix);

			range_string = vformat("(%s %s %s)", min_s, String::utf8("⋯"), max_s);
		} else {
			range_string = vformat("(%d%s %s %d%s)", range[0], suffix, String::utf8("⋯"), range[1], suffix);
		}
	}

	free(range);
	return range_string;
}

String MultiSpinBox::get_default_string() const {
	double *range = get_range();
	if (range == NULL)
		return String();

	String default_string;

	if (range[0] == range[1]) {
		// Using %.*f with a precision of 0 decimal places seems to silently fail
		if (precision > 0) {
			default_string = vformat("%.*f", precision, range[0]);
		} else {
			default_string = vformat("%d", range[0]);
		}
	} else {
		default_string = vformat("note.%s", property_name);
		if (inner_property_name.size() != 0)
			default_string = vformat("%s.%s", default_string, inner_property_name);
	}

	free(range);
	return default_string;
}

void MultiSpinBox::_text_entered(const String &p_string) {
	// Safeguard against trying to execute something when we are just showing the values
	if (p_string == get_range_string())
		return;

	String error_tooltip = String();

#ifdef MODULE_REGEX_ENABLED
	RegEx regex("\\[(.*?)\\]");

	if (regex.search(p_string).is_valid()) {
		warning = true;
		error_tooltip = String("WARNING: You are accessing an array.\nIf the index goes out of bounds, it will be clamped.");
	} else {
		warning = false;
	}

	String expression = regex.sub(p_string, "[clamp($1, 0, notes.size())]", true);
#else
	String expression = p_string;
#endif

	Ref<Expression> expr;
	expr.instantiate();

	Vector<String> input_names = Vector<String>();
	input_names.ordered_insert("i");
	input_names.ordered_insert("note");
	input_names.ordered_insert("notes");
	error = expr->parse(expression, input_names);
	if (error != OK) {
		error_tooltip = expr->get_error_text();
		queue_redraw();
		emit_signal("error_changed", error_tooltip);
		return;
	}

	values.clear();
	Array inputs_copy = inputs;
	for (int i = 0; i < inputs.size(); i++) {
		Variant input = inputs_copy[i];

		Array input_array = Array();
		input_array.append(i);
		input_array.append(input);
		input_array.append(inputs_copy);

		Variant value = expr->execute(input_array, nullptr, true);
		if (value.get_type() == Variant::FLOAT || value.get_type() == Variant::INT) {
			value = CLAMP((double)value, min, max);

			if (inner_property_name.size() != 0) {
				bool valid;
				Variant original_property = input.get_named(property_name, valid);
				ERR_FAIL_COND(!valid);
				original_property.set_named(inner_property_name, value, valid);
				ERR_FAIL_COND(!valid);
				inputs_copy[i].set_named(property_name, original_property, valid);
				ERR_FAIL_COND(!valid);
			} else {
				bool valid;
				inputs_copy[i].set_named(property_name, value, valid);
				ERR_FAIL_COND(!valid);
			}

			values[i] = value;
		} else {
			error = ERR_INVALID_DATA;
			String expr_error = expr->get_error_text();
			if (expr_error.size() == 0) {
				error_tooltip = vformat("ERROR: Invalid return type %s", value.get_type_name(value.get_type()));
			} else {
				error_tooltip = expr_error;
			}

			break;
		}
	}

	if (error == OK) {
		inputs = inputs_copy;
		emit_signal("values_changed");
		line_edit->release_focus();
	} else {
		reset_values();
	}

	emit_signal("error_changed", error_tooltip);
	queue_redraw();
}

Ref<Texture2D> MultiSpinBox::get_error_or_warning_icon() {
	if (error != OK) {
		return get_theme_icon("error");
	} else if (warning) {
		return get_theme_icon("warning");
	}

	return Ref<Texture>(NULL);
}

Dictionary MultiSpinBox::get_values() const {
	return values;
}

void MultiSpinBox::change_values(double by) {
	for (int i = 0; i < inputs.size(); i++) {
		Variant input = inputs[i];
		double value = CLAMP(double(values[i]) + by, min, max);
		values[i] = value;
		bool valid;
		if (inner_property_name.size() != 0) {
			Variant original_property = input.get_named(property_name, valid);
			ERR_FAIL_COND(!valid);
			original_property.set_named(inner_property_name, value, valid);
			ERR_FAIL_COND(!valid);

			inputs[i].set_named(property_name, original_property, valid);
			ERR_FAIL_COND(!valid);
		} else {
			inputs[i].set_named(property_name, value, valid);
			ERR_FAIL_COND(!valid);
		}
	}

	line_edit->set_text(get_range_string());
}

void MultiSpinBox::reset_values() {
	values.clear();
	for (int i = 0; i < inputs.size(); i++) {
		Variant input = inputs[i];
		bool valid;
		Variant property = input.get_named(property_name, valid);
		ERR_FAIL_COND(!valid);
		if (inner_property_name.size() != 0) {
			values[i] = property.get_named(inner_property_name, valid);
			ERR_FAIL_COND(!valid);
		} else {
			values[i] = property;
		}
	}
}

void MultiSpinBox::reset_expression() {
	line_edit->set_text(get_default_string());
	line_edit->release_focus();
}

void MultiSpinBox::set_property_name(String p_property_name) {
	property_name = p_property_name;

	line_edit->set_text(get_range_string());
}

String MultiSpinBox::get_property_name() const {
	return property_name;
}

void MultiSpinBox::set_inner_property_name(String p_inner_property_name) {
	inner_property_name = p_inner_property_name;

	line_edit->set_text(get_range_string());
}

String MultiSpinBox::get_inner_property_name() const {
	return inner_property_name;
}

void MultiSpinBox::set_suffix(String p_suffix) {
	suffix = p_suffix;

	line_edit->set_text(get_range_string());
}

String MultiSpinBox::get_suffix() const {
	return suffix;
}

void MultiSpinBox::set_precision(int p_precision) {
	precision = p_precision;
}

int MultiSpinBox::get_precision() const {
	return precision;
}

void MultiSpinBox::set_min(double p_min) {
	min = p_min;
}

double MultiSpinBox::get_min() const {
	return min;
}

void MultiSpinBox::set_max(double p_max) {
	max = p_max;
}

double MultiSpinBox::get_max() const {
	return max;
}

void MultiSpinBox::set_inputs(Array p_inputs) {
	inputs = p_inputs;

	reset_values();

	line_edit->set_text(get_range_string());
}

Array MultiSpinBox::get_inputs() const {
	return inputs;
}

LineEdit *MultiSpinBox::get_line_edit() {
	return line_edit;
}

void MultiSpinBox::_line_edit_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventKey> k = p_event;

	if (k.is_valid() && k->is_pressed()) {
		if (k->get_keycode() == Key::ESCAPE) {
			reset_expression();
		}
	}
}

void MultiSpinBox::_range_click_timeout() {
	if (Input::get_singleton()->is_mouse_button_pressed(MouseButton::LEFT)) {
		bool up = get_local_mouse_position().y < (get_size().height / 2);
		change_values(up ? 1.0 : -1.0);

		if (range_click_timer->is_one_shot()) {
			range_click_timer->set_wait_time(0.075);
			range_click_timer->set_one_shot(false);
			range_click_timer->start();
		}

	} else {
		range_click_timer->stop();
		emit_signal("values_changed");
	}
}

void MultiSpinBox::gui_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventMouseButton> mb = p_event;

	if (mb.is_valid() && mb->is_pressed() && mb->get_button_index() == MouseButton::LEFT) {
		if (mb->get_position().x > updown_pos && mb->get_position().x < updown_pos + updown_w) {
			bool up = mb->get_position().y < (get_size().height / 2);

			change_values(up ? 1.0 : -1.0);
			emit_signal("values_changed");

			range_click_timer->set_wait_time(0.6);
			range_click_timer->set_one_shot(true);
			range_click_timer->start();
		}
	}
}

void MultiSpinBox::_line_edit_focus_enter() {
	String default_string = get_default_string();
	line_edit->set_text(default_string);
	line_edit->set_caret_column(default_string.size());
}

void MultiSpinBox::_line_edit_focus_exit() {
	// discontinue because the focus_exit was caused by right-click context menu
	if (line_edit->get_menu()->is_visible()) {
		return;
	}

	line_edit->set_text(get_range_string());
	_text_entered(line_edit->get_text());
}

_FORCE_INLINE_ void MultiSpinBox::_adjust_width_for_icon(const Ref<Texture2D> &updown, const Ref<Texture2D> &warning_error) {
	updown_w = updown->get_width();
	error_w = warning_error.is_valid() ? warning_error->get_width() : 0;
	int w = updown_w + error_w;

	if (w != last_w) {
		line_edit->set_offset(Side::SIDE_RIGHT, -w);
		last_w = w;
	}
}

void MultiSpinBox::_notification(int p_what) {
	if (p_what == NOTIFICATION_DRAW) {
		Ref<Texture2D> updown = get_theme_icon(SNAME("updown"), SNAME("SpinBox"));
		Ref<Texture2D> error_warning = get_error_or_warning_icon();

		_adjust_width_for_icon(updown, error_warning);

		RID ci = get_canvas_item();
		Size2i size = get_size();
		error_pos = 0;
		updown_pos = size.width - updown_w;

		if (error_warning.is_valid()) {
			error_pos = size.width - error_w;
			updown_pos = error_pos - updown_w;

			error_warning->draw(ci, Point2i(error_pos, (size.height - error_warning->get_height()) / 2));
		}

		updown->draw(ci, Point2i(updown_pos, (size.height - updown->get_height()) / 2));
	} else if (p_what == NOTIFICATION_ENTER_TREE) {
		_adjust_width_for_icon(get_theme_icon(SNAME("updown"), SNAME("SpinBox")), get_error_or_warning_icon());
	} else if (p_what == NOTIFICATION_THEME_CHANGED) {
		call_deferred("minimum_size_changed");
		get_line_edit()->call_deferred("minimum_size_changed");
	}
}

void MultiSpinBox::set_align(HorizontalAlignment p_align) {
	line_edit->set_horizontal_alignment(p_align);
}

HorizontalAlignment MultiSpinBox::get_align() const {
	return line_edit->get_horizontal_alignment();
}

void MultiSpinBox::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_property_name", "property_name"), &MultiSpinBox::set_property_name);
	ClassDB::bind_method(D_METHOD("get_property_name"), &MultiSpinBox::get_property_name);

	ClassDB::bind_method(D_METHOD("set_inner_property_name", "inner_property_name"), &MultiSpinBox::set_inner_property_name);
	ClassDB::bind_method(D_METHOD("get_inner_property_name"), &MultiSpinBox::get_inner_property_name);

	ClassDB::bind_method(D_METHOD("set_suffix", "suffix"), &MultiSpinBox::set_suffix);
	ClassDB::bind_method(D_METHOD("get_suffix"), &MultiSpinBox::get_suffix);

	ClassDB::bind_method(D_METHOD("set_precision", "precision"), &MultiSpinBox::set_precision);
	ClassDB::bind_method(D_METHOD("get_precision"), &MultiSpinBox::get_precision);

	ClassDB::bind_method(D_METHOD("set_min", "min"), &MultiSpinBox::set_min);
	ClassDB::bind_method(D_METHOD("get_min"), &MultiSpinBox::get_min);

	ClassDB::bind_method(D_METHOD("set_max", "max"), &MultiSpinBox::set_max);
	ClassDB::bind_method(D_METHOD("get_max"), &MultiSpinBox::get_max);

	ClassDB::bind_method(D_METHOD("set_inputs", "inputs"), &MultiSpinBox::set_inputs);
	ClassDB::bind_method(D_METHOD("get_inputs"), &MultiSpinBox::get_inputs);

	ClassDB::bind_method(D_METHOD("set_align", "align"), &MultiSpinBox::set_align);
	ClassDB::bind_method(D_METHOD("get_align"), &MultiSpinBox::get_align);

	ClassDB::bind_method(D_METHOD("get_values"), &MultiSpinBox::get_values);

	ClassDB::bind_method(D_METHOD("execute", "expression"), &MultiSpinBox::_text_entered);
	ClassDB::bind_method(D_METHOD("reset_expression"), &MultiSpinBox::reset_expression);

	ClassDB::bind_method(D_METHOD("get_line_edit"), &MultiSpinBox::get_line_edit);
	ClassDB::bind_method(D_METHOD("_range_click_timeout"), &MultiSpinBox::_range_click_timeout);
	ClassDB::bind_method(D_METHOD("_text_entered"), &MultiSpinBox::_text_entered);
	ClassDB::bind_method(D_METHOD("_line_edit_focus_enter"), &MultiSpinBox::_line_edit_focus_enter);
	ClassDB::bind_method(D_METHOD("_line_edit_focus_exit"), &MultiSpinBox::_line_edit_focus_exit);
	ClassDB::bind_method(D_METHOD("_line_edit_input"), &MultiSpinBox::_line_edit_input);

	ADD_SIGNAL(MethodInfo("values_changed"));
	ADD_SIGNAL(MethodInfo("error_changed", PropertyInfo(Variant::STRING, "new_error")));

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "property_name"), "set_property_name", "get_property_name");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "inner_property_name"), "set_inner_property_name", "get_inner_property_name");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "suffix"), "set_suffix", "get_suffix");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "precision"), "set_precision", "get_precision");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "min"), "set_min", "get_min");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max"), "set_max", "get_max");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "inputs"), "set_inputs", "get_inputs");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "align", PROPERTY_HINT_ENUM, "Left,Center,Right,Fill"), "set_align", "get_align");
}

MultiSpinBox::MultiSpinBox() {
	values = Dictionary();
	property_name = String();
	inner_property_name = String();
	suffix = String();
	precision = 0;
	inputs = Array();

	min = 0;
	max = INFINITY;

	error = OK;
	warning = false;

	last_w = 0;
	updown_w = 0;
	error_w = 0;
	updown_pos = 0;
	error_pos = 0;

	line_edit = memnew(LineEdit);
	add_child(line_edit);

	line_edit->set_anchors_and_offsets_preset(LayoutPreset::PRESET_FULL_RECT);
	line_edit->set_mouse_filter(MOUSE_FILTER_PASS);
	line_edit->connect("text_submitted", callable_mp(this, &MultiSpinBox::_text_entered), CONNECT_DEFERRED);
	line_edit->connect("focus_entered", callable_mp(this, &MultiSpinBox::_line_edit_focus_enter), CONNECT_DEFERRED);
	line_edit->connect("focus_exited", callable_mp(this, &MultiSpinBox::_line_edit_focus_exit), CONNECT_DEFERRED);
	line_edit->connect("gui_input", callable_mp(this, &MultiSpinBox::_line_edit_input));

	range_click_timer = memnew(Timer);
	range_click_timer->connect("timeout", callable_mp(this, &MultiSpinBox::_range_click_timeout));
	add_child(range_click_timer);
}
