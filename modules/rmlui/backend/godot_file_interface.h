#pragma once

#include "core/io/file_access.h"
#include "RmlUi/Core/FileInterface.h"
#include "core/templates/hash_map.h"
#include "RmlUi/Core/Types.h"

namespace Rml {

class GodotFileInterface : public Rml::FileInterface {
    
    Rml::FileHandle Open(const Rml::String& path) override;
	void Close(Rml::FileHandle file) override;
	size_t Read(void* buffer, size_t size, Rml::FileHandle file) override;
	bool Seek(Rml::FileHandle file, long offset, int origin) override;
	size_t Tell(Rml::FileHandle file) override;

    HashMap<Rml::FileHandle, Ref<FileAccess>> open_handles;
};

}