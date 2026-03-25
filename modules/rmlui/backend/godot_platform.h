#pragma once

#include "RmlUi/Core/Input.h"
#include "RmlUi/Core/SystemInterface.h"
#include "RmlUi/Core/FileInterface.h"
#include "RmlUi/Core/Types.h"
#include "core/input/input_enums.h"
#include "core/input/input_event.h"
#include "core/os/keyboard.h"

class SystemInterface_Godot : public Rml::SystemInterface {
    double GetElapsedTime() override;

    void SetMouseCursor(const Rml::String &p_cursor_name) override;
	
    void SetClipboardText(const Rml::String& p_text) override;
	void GetClipboardText(Rml::String& p_text) override;

	void ActivateKeyboard(Rml::Vector2f caret_position, float line_height) override;
	void DeactivateKeyboard() override;
	int TranslateString(Rml::String& r_translated, const Rml::String& p_input) override;
};

namespace RmlGodot {
	int convert_mouse_button(MouseButton p_button);
	Rml::Input::KeyIdentifier convert_key(Key p_button);
	Rml::Input::KeyModifier get_key_modifiers();
	bool process_event(Rml::Context *p_context, const Ref<InputEvent> &p_event);
};