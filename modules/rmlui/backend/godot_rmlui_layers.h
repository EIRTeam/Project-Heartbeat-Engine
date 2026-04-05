#pragma once

#include "RmlUi/Core/Types.h"
#include "core/templates/rid.h"
#include "servers/rendering/rendering_device.h"

class GodotRmlUiLayers {
    RID framebuffer;
    RID shared_stencil_buffer;
    RID resolve_intermediary_buffer;
    RD::FramebufferFormatID out_fb_format;
    RD::FramebufferFormatID internal_fb_format = 0;
    struct Layer {
        RID framebuffer;
        RID color;
        bool allocated = false;
    };
    Vector<Layer*> allocated_layers;
    Vector<Layer*> idle_layers;
    Vector<Layer*> layer_stack;
    Layer *create_layer();
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
    RD::FramebufferFormatID get_framebuffer_format() const;
    ~GodotRmlUiLayers();
};