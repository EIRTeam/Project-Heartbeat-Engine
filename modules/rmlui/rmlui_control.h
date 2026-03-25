#pragma once

#include "scene/gui/control.h"
#include "servers/rendering/renderer_rd/storage_rd/render_canvas_data_rd.h"
class RmlUiControl : public Control {
    GDCLASS(RmlUiControl, Control);
    RID document;
public:
    static void _bind_methods();
    void load_document(const String &p_path);
    void _notification(int p_what);
    ~RmlUiControl();
};