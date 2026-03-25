#pragma once

#include "rmlui/thirdparty/RmlUi/Source/Core/FontEngineDefault/FontEngineInterfaceDefault.h"
#include "scene/resources/font.h"
class FontInterfaceGodot : public Rml::FontEngineInterfaceDefault {
public:
    bool LoadFontFace(const Rml::String& file_name, int face_index, bool fallback_face, Rml::Style::FontWeight weight) override;
    bool LoadFontFaceFromGodot(Ref<FontFile> p_font_file, int face_index, bool fallback_face, Rml::Style::FontWeight weight);
};