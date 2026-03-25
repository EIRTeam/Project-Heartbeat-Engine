#pragma once

#include "editor/inspector/editor_inspector.h"
#include "editor/plugins/editor_plugin.h"
#include "rmlui/extra/rmlui_sprite_sheet.h"
class RmlUiInspectorPlugin : public EditorInspectorPlugin {
    GDCLASS(RmlUiInspectorPlugin, EditorInspectorPlugin);

    virtual bool can_handle(Object *p_object) override;
	virtual void parse_begin(Object *p_object) override;
    void _build_spritesheet(Ref<RmlUiSpriteSheet> p_spritesheet);
};

class RmlUiEditorPlugin : public EditorPlugin {
    GDCLASS(RmlUiEditorPlugin, EditorPlugin);

    Ref<RmlUiInspectorPlugin> inspector_plugin;

    void _notification(int p_what);
};