#include "register_types.h"
#include "RmlUi/Core/Core.h"
#include "core/object/class_db.h"
#include "modules/register_module_types.h"
#include "rmlui/backend/godot_file_interface.h"
#include "rmlui/backend/godot_platform.h"
#include "rmlui/extra/rmlui_inspector_plugin.h"
#include "rmlui/extra/rmlui_sprite_sheet.h"
#include "rmlui/rmlui_control.h"
#include "rmlui/rmlui_singleton.h"

RmlUiSingleton *singleton = nullptr;

void initialize_rmlui_module(ModuleInitializationLevel p_level) {
#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		EditorPlugins::add_by_type<RmlUiEditorPlugin>();
	}
#endif

    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
    singleton = memnew(RmlUiSingleton);
    GDREGISTER_CLASS(RmlUiControl);
    GDREGISTER_CLASS(RmlUiSpriteSheet);
    GDREGISTER_CLASS(RmlUiSpriteSheetTexture);
    GDREGISTER_ABSTRACT_CLASS(RmlUiSingleton);
    GDREGISTER_CLASS(RmlUiEditorPlugin);
    GDREGISTER_CLASS(RmlUiInspectorPlugin);
    Engine::get_singleton()->add_singleton(Engine::Singleton("RmlUi", singleton));
}

void uninitialize_rmlui_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
    memdelete(singleton);
}
