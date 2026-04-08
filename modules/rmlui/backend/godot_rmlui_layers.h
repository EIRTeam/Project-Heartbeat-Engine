#pragma once

#include "RmlUi/Core/Types.h"
#include "core/templates/rid.h"
#include "servers/rendering/rendering_device.h"
#include <optional>

class GodotRmlUiLayers {
    RID framebuffer;
    RID shared_stencil_buffer;
    RID resolve_intermediary_buffer;
    RD::FramebufferFormatID out_fb_format;
    RD::FramebufferFormatID internal_fb_format = 0;
public:
    struct AllocatedFramebuffer {
        RID framebuffer;
        RID color_texture;
    };
private:
    struct Layer {
        std::optional<AllocatedFramebuffer> framebuffer;
        int idx = 0;
    };
    Vector<Layer*> allocated_layers;
    Vector<Layer*> layer_stack;
    
    static constexpr int BACKBUFFER_COUNT = 2;
    Layer backbuffers[BACKBUFFER_COUNT];
    static void _fb_invalidation(void *p_userdata);

    // Data used for preprocessing the frame, we only do allocations
    // on the render thread, but we need to alocate Layer objects ahead of time.
    struct Preprocess {
        int max_depth = 0;
        int current_preprocess_layer = 0;
    } preprocess;

public:
    void init();
    void recreate_shared_textures();
    void clear_layers();
    void frame_start(RID p_final_framebuffer);
    void render_start();
    Layer *prepush_layer();
    void prepop_layer();
    void push_layer();
    void pop_layer();
    Rml::LayerHandle get_layer(int p_layer) const;
    RID get_layer_color_texture(int p_layer) const;
    RID get_layer_framebuffer(int p_layer) const;
    int get_current_layer() const;
    RID get_resolve_buffer() const;
    int layer_get_idx(Rml::LayerHandle p_layer) const;
    RD::FramebufferFormatID get_framebuffer_format() const;
    AllocatedFramebuffer allocate_framebuffer(const String &p_hint_name) const;

    RID backbuffer_get_framebuffer(int p_idx) const;
    RID backbuffer_get_color_texture(int p_idx) const;
    ~GodotRmlUiLayers();
};