#include "rmlui_inspector_plugin.h"
#include "rmlui/extra/packer.h"
#include "rmlui/extra/rmlui_sprite_sheet.h"
#include "editor/file_system/editor_file_system.h"
#include "scene/main/node.h"

bool RmlUiInspectorPlugin::can_handle(Object *p_object) {
    return Object::cast_to<RmlUiSpriteSheet>(p_object) != nullptr;
}

void RmlUiInspectorPlugin::parse_begin(Object *p_object){
    Ref<RmlUiSpriteSheet> sprite_sheet = Object::cast_to<RmlUiSpriteSheet>(p_object);
    Button *button = memnew(Button);
    button->set_text("Rebuild");
    button->connect("pressed", callable_mp(this, &RmlUiInspectorPlugin::_build_spritesheet).bind(sprite_sheet));
    add_custom_control(button);
}

void RmlUiInspectorPlugin::_build_spritesheet(Ref<RmlUiSpriteSheet> p_spritesheet)
{
    RmlUiPacker packer = RmlUiPacker({
        .sheet = p_spritesheet
    });
    packer.pack();
    EditorFileSystem::get_singleton()->scan();
}

void RmlUiEditorPlugin::_notification(int p_what) {
    switch(p_what) {
        case NOTIFICATION_ENTER_TREE: {
            inspector_plugin.instantiate();
            add_inspector_plugin(inspector_plugin);
        } break;
        case NOTIFICATION_EXIT_TREE: {
            remove_inspector_plugin(inspector_plugin);
        } break;
    }
}


