#ifndef MULTI_SPIN_BOX
#define MULTI_SPIN_BOX

#include "scene/gui/line_edit.h"

class MultiSpinBox : public Control {
	GDCLASS(MultiSpinBox, Control);

	LineEdit *line_edit;
	int last_w;
	int updown_w;
	int error_w;
	int updown_pos;
	int error_pos;

	double *get_range() const;
	String get_range_string() const;
	String get_default_string() const;

	Timer *range_click_timer;
	void _range_click_timeout();

	Dictionary values;
	String property_name;
	String inner_property_name;
	String suffix;
	int precision;
	double min;
	double max;

	Array inputs;

	Error error;
	bool warning;

	Ref<Texture2D> get_error_or_warning_icon();

	void change_values(double by);
	void reset_values();

	void _line_edit_focus_enter();
	void _line_edit_focus_exit();
	void _line_edit_input(const Ref<InputEvent> &event);
	void _line_edit_gui_input(const Ref<InputEvent> &event);

	void _text_entered(const String &p_string);

	_FORCE_INLINE_ void _adjust_width_for_icon(const Ref<Texture2D> &updown, const Ref<Texture2D> &error_warning);

protected:
	virtual void gui_input(const Ref<InputEvent> &p_event) override;

	void _notification(int p_what);

	static void _bind_methods();

public:
	LineEdit *get_line_edit();

	virtual Size2 get_minimum_size() const override;

	Dictionary get_values() const;

	void reset_expression();

	void set_property_name(String p_property_name);
	String get_property_name() const;

	void set_inner_property_name(String p_inner_property_name);
	String get_inner_property_name() const;

	void set_suffix(String p_suffix);
	String get_suffix() const;

	void set_precision(int p_precision);
	int get_precision() const;

	void set_min(double p_min);
	double get_min() const;

	void set_max(double p_max);
	double get_max() const;

	void set_inputs(Array p_inputs);
	Array get_inputs() const;

	void set_align(HorizontalAlignment p_align);
	HorizontalAlignment get_align() const;

	MultiSpinBox();
};

#endif
