#include "rmlui_control.h"
#include "rmlui/rmlui_singleton.h"

void RmlUiControl::_bind_methods() {
    ClassDB::bind_method(D_METHOD("load_document", "path"), &RmlUiControl::load_document);
}

void RmlUiControl::load_document(const String &p_path) {
    document = RmlUiSingleton::get_singleton()->create_document_from_path(p_path);

}

void RmlUiControl::_notification(int p_what) {
}

RmlUiControl::~RmlUiControl() {
    if (document.is_valid()) {
        RmlUiSingleton::get_singleton()->free_document(document);
    }
}
