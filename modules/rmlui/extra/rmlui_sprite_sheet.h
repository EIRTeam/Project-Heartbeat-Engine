#pragma once

#include "core/io/resource.h"
#include "scene/resources/texture.h"

class RmlUiSpriteSheetTexture : public Resource {
    GDCLASS(RmlUiSpriteSheetTexture, Resource);
    String texture;
    String name;

public:
    static void _bind_methods();
    String get_texture() const;
    void set_texture(const String &p_texture);

    String get_sprite_name() const;
    void set_sprite_name(const String &p_name);
};

class RmlUiSpriteSheet : public Resource {
    GDCLASS(RmlUiSpriteSheet, Resource);
    String target_path;
    String name;
    Vector<Ref<RmlUiSpriteSheetTexture>> textures;
    int margin = 0;
    static void _bind_methods();

    void set_textures(TypedArray<RmlUiSpriteSheetTexture> p_texture);
    TypedArray<RmlUiSpriteSheetTexture> get_textures() const;

public:
    String get_spritesheet_name() const;
    void set_spritesheet_name(const String &p_name);

    String get_target_path() const;
    void set_target_path(const String &p_target_path);

    void set_margin(int p_margin);
    int get_margin() const;

    friend class RmlUiPacker;
};