#ifndef PH_BLUR_CONTROLS_H
#define PH_BLUR_CONTROLS_H

#include "scene/gui/button.h"
#include "scene/gui/panel_container.h"

class HBStyleboxBlurDrawer : public Control {
	GDCLASS(HBStyleboxBlurDrawer, Control);
	Ref<StyleBox> stylebox;
	Ref<StyleBox> stylebox_original;

	void _update_stylebox();

protected:
	void _on_blur_controls_enabled_changed();

public:
	static Ref<ShaderMaterial> blur_material;
	void set_stylebox_original(Ref<StyleBox> p_stylebox);
	void _notification(int p_what);

	HBStyleboxBlurDrawer();
};

class HBPanelContainerBlurEX : public PanelContainer {
	GDCLASS(HBPanelContainerBlurEX, PanelContainer);
	HBStyleboxBlurDrawer *blur_drawer = nullptr;

public:
	void _notification(int p_what);
	HBPanelContainerBlurEX();
};

class HBPanelBlurEX : public PanelContainer {
	GDCLASS(HBPanelBlurEX, PanelContainer);
	HBStyleboxBlurDrawer *blur_drawer = nullptr;

public:
	void _notification(int p_what);
	HBPanelBlurEX();
};

class HBButtonBlurEX : public Button {
	GDCLASS(HBButtonBlurEX, Button);

	HBStyleboxBlurDrawer *blur_drawer = nullptr;

protected:
	void _notification(int p_what);

public:
	HBButtonBlurEX();
};

#endif // PH_BUTTON_EX_H
