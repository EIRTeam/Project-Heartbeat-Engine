#include "rmlui_sprite_sheet.h"
#include "core/object/class_db.h"
#include "core/object/object.h"

/*
Sheet
*/

void RmlUiSpriteSheet::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_textures", "textures"), &RmlUiSpriteSheet::set_textures);
    ClassDB::bind_method(D_METHOD("get_textures"), &RmlUiSpriteSheet::get_textures);
    ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "textures", PROPERTY_HINT_ARRAY_TYPE, "RmlUiSpriteSheetTexture"), "set_textures", "get_textures");

    ClassDB::bind_method(D_METHOD("set_spritesheet_name", "name"), &RmlUiSpriteSheet::set_spritesheet_name);
    ClassDB::bind_method(D_METHOD("get_spritesheet_name"), &RmlUiSpriteSheet::get_spritesheet_name);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "spritesheet_name"), "set_spritesheet_name", "get_spritesheet_name");

    ClassDB::bind_method(D_METHOD("set_margin", "margin"), &RmlUiSpriteSheet::set_margin);
    ClassDB::bind_method(D_METHOD("get_margin"), &RmlUiSpriteSheet::get_margin);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "margin", PROPERTY_HINT_RANGE, "0,10,1"), "set_margin", "get_margin");
    
    ClassDB::bind_method(D_METHOD("set_target_path", "target_path"), &RmlUiSpriteSheet::set_target_path);
    ClassDB::bind_method(D_METHOD("get_target_path"), &RmlUiSpriteSheet::get_target_path);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "target_path", PROPERTY_HINT_SAVE_FILE, "*.rcss"), "set_target_path", "get_target_path");
}

void RmlUiSpriteSheet::set_textures(TypedArray<RmlUiSpriteSheetTexture> p_texture) {
    textures.resize(p_texture.size());

    Ref<RmlUiSpriteSheetTexture> *texture = textures.ptrw();

    for (int i = 0; i < textures.size(); i++) {
        texture[i] = p_texture[i];
    }
}

TypedArray<RmlUiSpriteSheetTexture> RmlUiSpriteSheet::get_textures() const {
    TypedArray<RmlUiSpriteSheetTexture> textures_out;
    textures_out.resize(textures.size());
    for (int i = 0; i < textures.size(); i++) {
        textures_out[i] = textures[i];
    }

    return textures_out;
}

String RmlUiSpriteSheet::get_spritesheet_name() const {
    return name;
}

void RmlUiSpriteSheet::set_spritesheet_name(const String &p_name) {
    name = p_name;
}

String RmlUiSpriteSheet::get_target_path() const
{
    return target_path;
}

void RmlUiSpriteSheet::set_target_path(const String &p_target_path)
{
    target_path = p_target_path;
}

void RmlUiSpriteSheet::set_margin(int p_margin) { margin = p_margin; }

int RmlUiSpriteSheet::get_margin() const { return margin; }

/*
Individual sprite
*/

void RmlUiSpriteSheetTexture::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_sprite_name", "name"), &RmlUiSpriteSheetTexture::set_sprite_name);
    ClassDB::bind_method(D_METHOD("get_sprite_name"), &RmlUiSpriteSheetTexture::get_sprite_name);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "sprite_name"), "set_sprite_name", "get_sprite_name");

    ClassDB::bind_method(D_METHOD("set_texture", "texture"), &RmlUiSpriteSheetTexture::set_texture);
    ClassDB::bind_method(D_METHOD("get_texture"), &RmlUiSpriteSheetTexture::get_texture);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "texture", PROPERTY_HINT_FILE_PATH, "*.png"), "set_texture", "get_texture");
}

String RmlUiSpriteSheetTexture::get_texture() const {
    return texture;
}

void RmlUiSpriteSheetTexture::set_texture(const String &p_texture) {
    texture = p_texture;
}

String RmlUiSpriteSheetTexture::get_sprite_name() const {
    return name;
}

void RmlUiSpriteSheetTexture::set_sprite_name(const String &p_name) {
    name = p_name;
}
