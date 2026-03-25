#include "godot_font_interface.h"
#include "RmlUi/Core/StyleTypes.h"
#include "core/error/error_macros.h"
#include "core/io/resource_loader.h"

#include "core/variant/variant.h"
#include "godot_conversion.h"
#include "rmlui/thirdparty/RmlUi/Source/Core/FontEngineDefault/FontEngineInterfaceDefault.h"
#include "scene/resources/font.h"

bool FontInterfaceGodot::LoadFontFace(const Rml::String& file_name, int face_index, bool fallback_face, Rml::Style::FontWeight weight) {
    Ref<FontFile> font = ResourceLoader::load(to_godot(file_name));

    return LoadFontFaceFromGodot(font, face_index, fallback_face, weight);
}

bool FontInterfaceGodot::LoadFontFaceFromGodot(Ref<FontFile> p_font_file, int face_index, bool fallback_face, Rml::Style::FontWeight weight) {
    PackedByteArray font_data = p_font_file->get_data();

    Rml::Span<const Rml::byte> font_data_rml = Rml::Span<const Rml::byte>(font_data.ptr(), font_data.size());

    Rml::Style::FontStyle font_style = Rml::Style::FontStyle::Normal;

    if (p_font_file->get_font_style().has_flag(TextServer::FontStyle::FONT_ITALIC)) {
        font_style = Rml::Style::FontStyle::Italic;
    }

    return Rml::FontEngineInterfaceDefault::LoadFontFace(font_data_rml, 0, from_godot(p_font_file->get_font_name()), font_style, weight, fallback_face);
}
