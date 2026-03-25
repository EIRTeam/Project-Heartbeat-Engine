#pragma once

#include "core/error/error_list.h"
#include "rmlui/extra/rmlui_sprite_sheet.h"
class RmlUiPacker {
public:
    struct PackerParams {
        Ref<RmlUiSpriteSheet> sheet;
    };

    PackerParams params;
    RmlUiPacker(const PackerParams &p_params);
    Error pack();
};