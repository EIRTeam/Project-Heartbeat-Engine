#include "rmlui_singleton.h"
#include "RmlUi/Core/Core.h"
#include "RmlUi/Debugger/Debugger.h"
#include "core/variant/variant.h"
#include "rmlui/backend/godot_platform.h"
#include "rmlui/backend/godot_render_interface.h"
#include "rmlui/thirdparty/RmlUi/Include/RmlUi/Core/Context.h"
#include "rmlui/thirdparty/RmlUi/Include/RmlUi/Core/Core.h"
#include "rmlui/thirdparty/RmlUi/Include/RmlUi/Core/StyleTypes.h"
#include "rmlui/thirdparty/RmlUi/Source/Core/FontEngineDefault/FontEngineInterfaceDefault.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "scene/resources/world_2d.h"
#include "servers/display/display_server.h"
#include "RmlUi/Debugger.h"
#include "backend/godot_conversion.h"

void RmlUiSingleton::_bind_methods() {
    ClassDB::bind_method(D_METHOD("init"), &RmlUiSingleton::init);
    ClassDB::bind_method(D_METHOD("begin_frame"), &RmlUiSingleton::begin_frame);
    ClassDB::bind_method(D_METHOD("end_frame"), &RmlUiSingleton::end_frame);
}
void RmlUiSingleton::init() {
    Rml::SetFileInterface(&file_interface);
    Rml::SetSystemInterface(&system_interface);
    render_interface = memnew(RenderInterface_Godot_RD);
    render_interface->Initialize();
    Rml::SetRenderInterface(render_interface);
    Rml::SetFontEngineInterface(&font_interface);

    Rml::Initialise();

    Ref<FontFile> font = SceneTree::get_singleton()->get_root()->get_theme_default_font();
    font_interface.LoadFontFaceFromGodot(font, 0, true, Rml::Style::FontWeight::Auto);
    Vector2i window_size = DisplayServer::get_singleton()->window_get_size();
    context = Rml::CreateContext(Rml::String("penis"), Rml::Vector2i(window_size.x, window_size.y));

    Rml::Debugger::Initialise(context);
    Rml::Debugger::SetVisible(true);
    RS *rs = RS::get_singleton();
    canvas_item = rs->canvas_item_create();
    RID canvas = SceneTree::get_singleton()->get_root()->get_world_2d()->get_canvas();
    rs->canvas_item_set_parent(canvas_item, canvas);

    SceneTree::get_singleton()->get_root()->connect("window_input", callable_mp(this, &RmlUiSingleton::_on_input));
    RenderingServer::get_singleton()->canvas_item_add_rendering_callback(canvas_item, callable_mp(this, &RmlUiSingleton::_draw_context_commands));
}
void RmlUiSingleton::begin_frame() {
    const Vector2i window_size = DisplayServer::get_singleton()->window_get_size();
    context->SetDimensions(Rml::Vector2i(
        window_size.x,
        window_size.y
    ));
    context->Update();
    Projection proj = Projection::create_orthogonal(0.0, window_size.x, 0, window_size.y, -100.0f, 100.0f);
    render_interface->set_projection(proj);

    render_interface->BeginFrame();
    context->Render();
}

void RmlUiSingleton::_draw_context_commands(RenderCanvasDataRD *p_data) {
    render_interface->render();
}

void RmlUiSingleton::_on_input(const Ref<InputEvent> &p_event) {
    if (RmlGodot::process_event(context, p_event)) {
        SceneTree::get_singleton()->get_root()->set_input_as_handled();
    }
}

void RmlUiSingleton::end_frame() {
}

void RmlUiSingleton::_end_frame() {
    render_interface->EndFrame();
}

RmlUiSingleton *RmlUiSingleton::get_singleton()
{
    return singleton;
}

RID RmlUiSingleton::create_document_from_path(const String &p_path) {
    RmlDocument doc;
    doc.document = context->LoadDocument(from_godot(p_path));
    RID rid = document_owner.allocate_rid();
    document_owner.initialize_rid(rid, doc);
    doc.document->Show();
    return rid;
}

void RmlUiSingleton::attach_document_to_control(const RID &p_document, Control *p_control) {
	ERR_FAIL_NULL(p_control);
    RmlDocument *document = document_owner.get_or_null(p_document);
	ERR_FAIL_NULL(document);

    document->control = (p_control->get_instance_id());
}

void RmlUiSingleton::free_document(const RID &p_document)
{
    RmlDocument *document = document_owner.get_or_null(p_document);
	ERR_FAIL_NULL(document);
    context->UnloadDocument(document->document);
    document_owner.free(p_document);
}

RmlUiSingleton::RmlUiSingleton() {
    singleton = this;
}

RmlUiSingleton::~RmlUiSingleton() {
    if (!initialized) {
        return;
    }
    if (canvas_item.is_valid()) {
        RS::get_singleton()->free_rid(canvas_item);
    }

    Rml::Shutdown();

    if (render_interface != nullptr) {
        memdelete(render_interface);
    }
}
