#include "godot_platform.h"
#include "RmlUi/Core/Input.h"
#include "RmlUi/Core/StringUtilities.h"
#include "RmlUi/Core/Context.h"
#include "core/error/error_macros.h"
#include "core/input/input_enums.h"
#include "core/os/time.h"
#include "core/os/os.h"
#include "core/input/input.h"
#include "core/string/translation_server.h"
#include "godot_conversion.h"
#include "servers/display/display_server.h"


double SystemInterface_Godot::GetElapsedTime() {
    return Time::get_singleton()->get_ticks_usec() / 1000000.0;
}

void SystemInterface_Godot::SetMouseCursor(const Rml::String &p_cursor_name) {

    DisplayServer::CursorShape shape = DisplayServer::CURSOR_ARROW; 

    if (p_cursor_name == "move") {
        shape = DisplayServer::CURSOR_MOVE;
    } else if (p_cursor_name == "pointer") {
        shape = DisplayServer::CURSOR_POINTING_HAND;
    } else if (p_cursor_name == "resize") {
        shape = DisplayServer::CURSOR_BDIAGSIZE;
    } else if (p_cursor_name == "cross") {
        shape = DisplayServer::CURSOR_CROSS;
    } else if (p_cursor_name == "text") {
        shape = DisplayServer::CURSOR_IBEAM;
    } else if (p_cursor_name == "unavailable") {
        shape = DisplayServer::CURSOR_FORBIDDEN;
    } else if (Rml::StringUtilities::StartsWith(p_cursor_name, "rmlui-scroll")) {
        shape = DisplayServer::CURSOR_MOVE;
    } else if (p_cursor_name != "arrow" && !p_cursor_name.empty()) {
        ERR_FAIL_MSG(vformat("Unknown mouse cursor type %s", to_godot(p_cursor_name)));
    }

    DisplayServer::get_singleton()->cursor_set_shape(shape);
}

void SystemInterface_Godot::SetClipboardText(const Rml::String& p_text) {
    DisplayServer::get_singleton()->clipboard_set(to_godot(p_text));
}

void SystemInterface_Godot::GetClipboardText(Rml::String& p_text) {
    p_text = from_godot(DisplayServer::get_singleton()->clipboard_get());
}

void SystemInterface_Godot::ActivateKeyboard(Rml::Vector2f caret_position, float line_height) {
    
}

void SystemInterface_Godot::DeactivateKeyboard() {
    
}

int SystemInterface_Godot::TranslateString(Rml::String& r_translated, const Rml::String& p_input) {
    StringName in = to_godot(p_input);
    StringName out = TranslationServer::get_singleton()->translate(in);
    r_translated = from_godot(out);

    if (in == out) {
        return 0;
    }

    return 1;
}

int RmlGodot::convert_mouse_button(MouseButton p_button) {
	switch(p_button) {
		case MouseButton::LEFT: {
            return 0;
        }
		case MouseButton::RIGHT: {
            return 1;
        } break;
		case MouseButton::MIDDLE: {
            return 2;
        } break;
        default: {
            return 3;
        }
	}
}

Rml::Input::KeyIdentifier RmlGodot::convert_key(Key p_button) {
	if (p_button >= Key::A && p_button <= Key::Z) {
        return (Rml::Input::KeyIdentifier)((int)Rml::Input::KI_A + (int)(p_button - Key::A));
    }
	if (p_button >= Key::KP_0 && p_button <= Key::KP_9) {
        return (Rml::Input::KeyIdentifier)((int)Rml::Input::KI_NUMPAD0 + (int)(p_button - Key::KP_0));
    }
	if (p_button >= Key::F1 && p_button <= Key::F24) {
        return (Rml::Input::KeyIdentifier)((int)Rml::Input::KI_F1 + (int)(p_button - Key::F1));
    }
	if (p_button >= Key::KEY_0 && p_button <= Key::KEY_9) {
        return (Rml::Input::KeyIdentifier)((int)Rml::Input::KI_0 + (int)(p_button - Key::KEY_0));
    }

    switch (p_button) {
        case Key::CTRL: return Rml::Input::KI_LCONTROL;
        case Key::SHIFT: return Rml::Input::KI_LSHIFT;
        case Key::INSERT: return Rml::Input::KI_INSERT;
        case Key::KEY_DELETE: return Rml::Input::KI_DELETE;
        case Key::PAGEUP: return Rml::Input::KI_PRIOR;
        case Key::PAGEDOWN: return Rml::Input::KI_NEXT;
        case Key::LEFT: return Rml::Input::KI_LEFT;
        case Key::UP: return Rml::Input::KI_UP;
        case Key::DOWN: return Rml::Input::KI_DOWN;
        case Key::RIGHT: return Rml::Input::KI_RIGHT;
        case Key::HOME: return Rml::Input::KI_HOME;
        case Key::END: return Rml::Input::KI_END;
        case Key::ESCAPE: return Rml::Input::KI_ESCAPE;
        case Key::ENTER: return Rml::Input::KI_RETURN;
        case Key::BACKSPACE: return Rml::Input::KI_BACK;
        case Key::TAB: return Rml::Input::KI_TAB;
        case Key::SPACE: return Rml::Input::KI_SPACE;
    }

    return Rml::Input::KI_UNKNOWN;
}

Rml::Input::KeyModifier RmlGodot::get_key_modifiers() {
	return Rml::Input::KeyModifier();
}



bool RmlGodot::process_event(Rml::Context *p_context, const Ref<InputEvent> &p_event) {
    bool result = true;
    if (Ref<InputEventMouseMotion> motion = p_event; motion.is_valid()) {
        const Vector2 global_pos = motion->get_global_position();
        result = p_context->ProcessMouseMove(global_pos.x, global_pos.y, get_key_modifiers());
    } else if (Ref<InputEventMouseButton> mouse_button = p_event; mouse_button.is_valid()) {
        const MouseButton button_idx = mouse_button->get_button_index();
        if (button_idx == MouseButton::WHEEL_DOWN || button_idx == MouseButton::WHEEL_UP) {
            const float factor = button_idx == MouseButton::WHEEL_UP ? mouse_button->get_factor() : -mouse_button->get_factor();
            result = p_context->ProcessMouseWheel(factor, get_key_modifiers());
        } else if (mouse_button->is_pressed()) {
            result = p_context->ProcessMouseButtonDown(convert_mouse_button(button_idx), get_key_modifiers());
        } else {
            result = p_context->ProcessMouseButtonUp(convert_mouse_button(button_idx), get_key_modifiers());
        }
    } else if (Ref<InputEventKey> key = p_event; key.is_valid()) {
        p_context->ProcessKeyDown(convert_key(key->get_physical_keycode()), get_key_modifiers());
    }

    return result;
}
