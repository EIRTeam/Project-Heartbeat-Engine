#include "RmlUi/Config/Config.h"
#include "core/string/ustring.h"
#include "core/typedefs.h"
#include <string>

_FORCE_INLINE_ Rml::String from_godot(const String &p_string) {
    return Rml::String(p_string.utf8().get_data());
};

_FORCE_INLINE_ String to_godot(const Rml::String &p_string) {
    return String::utf8(p_string.c_str(), (int)p_string.length());
}