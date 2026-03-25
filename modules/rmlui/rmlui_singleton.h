#pragma once

#include "core/object/object.h"
#include "backend/godot_file_interface.h"
#include "backend/godot_platform.h"
#include "core/templates/rid_owner.h"
#include "rmlui/backend/godot_font_interface.h"
#include "rmlui/backend/godot_render_interface.h"
#include "rmlui/thirdparty/RmlUi/Include/RmlUi/Core/ElementDocument.h"
#include "rmlui/thirdparty/RmlUi/Source/Core/FontEngineDefault/FontEngineInterfaceDefault.h"
#include "scene/gui/control.h"
#include "servers/rendering/renderer_rd/storage_rd/render_canvas_data_rd.h"

class RmlUiSingleton : public Object {
    GDCLASS(RmlUiSingleton, Object);

    static void _bind_methods();

    struct RmlDocument {
        Rml::ElementDocument *document = nullptr;
        ObjectID control;
    };

    RID_Owner<RmlDocument> document_owner;

    void init();
    void begin_frame();
    void end_frame();
    void _end_frame();

    Rml::GodotFileInterface file_interface;
    SystemInterface_Godot system_interface;
    RenderInterface_Godot_RD *render_interface = nullptr;
    FontInterfaceGodot font_interface;
    static inline RmlUiSingleton *singleton = nullptr;
    Rml::Context *context = nullptr;
    RID canvas_item;

    bool initialized = false;
public:
    static RmlUiSingleton *get_singleton();
    RID create_document_from_path(const String &p_path);
    void attach_document_to_control(const RID &p_document, Control *p_control);
    void free_document(const RID &p_document);
    void _draw_context_commands(RenderCanvasDataRD *p_data);
    void _on_input(const Ref<InputEvent> &p_event);
    RmlUiSingleton();
    ~RmlUiSingleton();
};