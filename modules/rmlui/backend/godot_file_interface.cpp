#include "godot_file_interface.h"
#include "core/error/error_macros.h"
#include "godot_conversion.h"
#include <cstdio>

Rml::FileHandle Rml::GodotFileInterface::Open(const Rml::String& p_path) {
    Ref<FileAccess> file = FileAccess::open(to_godot(p_path), FileAccess::READ);
    if (file.is_null()) {
        return (Rml::FileHandle)nullptr;
    }

    Rml::FileHandle handle = (Rml::FileHandle)file.ptr();

    open_handles.insert(handle, file);

    return handle;
}

void Rml::GodotFileInterface::Close(Rml::FileHandle file) {
    auto it = open_handles.find(file);
    if (it != open_handles.end()) {
        open_handles.remove(it);
    }
}

size_t Rml::GodotFileInterface::Read(void* buffer, size_t size, Rml::FileHandle file) {
    auto it = open_handles.find(file);
    ERR_FAIL_COND_V_MSG(it == open_handles.end(), 0, "Tried to read from invalid file handle!");
    return it->value->get_buffer(static_cast<uint8_t*>(buffer), size);
}

bool Rml::GodotFileInterface::Seek(Rml::FileHandle file, long offset, int origin) {
    auto it = open_handles.find(file);
    ERR_FAIL_COND_V_MSG(it == open_handles.end(), false, "Tried to read from invalid file handle!");
    switch(origin) {
        case SEEK_SET: {
            it->value->seek(offset);
        } break;
        case SEEK_CUR: {
            it->value->seek(it->value->get_position() + offset);
        } break;
        case SEEK_END: {
            it->value->seek_end(offset);
        } break;
        default: {
            ERR_FAIL_V_MSG(false, "Error seeking: Invalid seek origin");
        } break;
    }
    return true;
}

size_t Rml::GodotFileInterface::Tell(Rml::FileHandle file) {
    auto it = open_handles.find(file);
    ERR_FAIL_COND_V_MSG(it == open_handles.end(), 0, "Tried to read from invalid file handle!");
    return it->value->get_position();
}
