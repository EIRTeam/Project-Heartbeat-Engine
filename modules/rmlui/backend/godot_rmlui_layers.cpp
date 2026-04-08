#include "godot_rmlui_layers.h"

#include "core/error/error_macros.h"
#include "servers/rendering/renderer_rd/storage_rd/render_scene_buffers_rd.h"
#include "servers/rendering/rendering_device_commons.h"

void GodotRmlUiLayers::_fb_invalidation(void *p_userdata) {
    GodotRmlUiLayers *layers = static_cast<GodotRmlUiLayers*>(p_userdata);
    layers->clear_layers();
}

void GodotRmlUiLayers::init() {
    preprocess.current_preprocess_layer = -1;
    prepush_layer();
}

void GodotRmlUiLayers::recreate_shared_textures() {
    RD *rd = RD::get_singleton();
    const RD::DataFormat depth_data_format = RenderSceneBuffersRD::get_depth_format(false, false, true);
    const Vector2i size = rd->framebuffer_get_size(framebuffer);
    RD::TextureFormat depth_format = {
        .format = depth_data_format,
        .width = static_cast<uint32_t>(size.x),
        .height = static_cast<uint32_t>(size.y),
        .usage_bits = RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
    };

    RD::TextureView texture_view = {};
    shared_stencil_buffer = rd->texture_create(depth_format, texture_view);
    const RD::DataFormat color_data_format = RenderingDeviceCommons::DATA_FORMAT_R8G8B8A8_UNORM;
    RD::TextureFormat resolve_format = {
        .format = color_data_format,
        .width = static_cast<uint32_t>(size.x),
        .height = static_cast<uint32_t>(size.y),
        .usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT
    };

    resolve_intermediary_buffer = rd->texture_create(resolve_format, texture_view);
}

void GodotRmlUiLayers::clear_layers() {
    DEV_ASSERT(layer_stack.is_empty());
    RD *rd = RD::get_singleton();
    if (shared_stencil_buffer.is_valid()) {
        rd->free_rid(shared_stencil_buffer);
        shared_stencil_buffer = RID();
    }

    if (resolve_intermediary_buffer.is_valid()) {
        rd->free_rid(resolve_intermediary_buffer);
        resolve_intermediary_buffer = RID();
    }
    for (Layer * layer : allocated_layers) {
        if (!layer->framebuffer.has_value()) {
            continue;
        }
        rd->free_rid(layer->framebuffer->color_texture);
        layer->framebuffer.reset();
    }

    for (Layer &layer : backbuffers) {
        if (!layer.framebuffer.has_value()) {
            continue;
        }
        rd->free_rid(layer.framebuffer->color_texture);
        layer.framebuffer.reset();
    }
}

void GodotRmlUiLayers::frame_start(RID p_out_framebuffer_id) {
    framebuffer = p_out_framebuffer_id;
    
    if (p_out_framebuffer_id != framebuffer || out_fb_format != RD::get_singleton()->framebuffer_get_format(p_out_framebuffer_id)) {
        framebuffer = p_out_framebuffer_id;
        out_fb_format = RD::get_singleton()->framebuffer_get_format(p_out_framebuffer_id);
        clear_layers();
        RD::get_singleton()->framebuffer_set_invalidation_callback(p_out_framebuffer_id, &GodotRmlUiLayers::_fb_invalidation, this);
        
        RD::AttachmentFormat color_attachment_format;
        color_attachment_format.format = RenderingDeviceCommons::DATA_FORMAT_R8G8B8A8_UNORM;
        color_attachment_format.samples = RenderingDeviceCommons::TEXTURE_SAMPLES_1;
        color_attachment_format.usage_flags = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_CAN_COPY_TO_BIT | RD::TEXTURE_USAGE_STORAGE_BIT | RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT;

        const RD::DataFormat depth_data_format = RenderSceneBuffersRD::get_depth_format(false, false, true);
        RD::AttachmentFormat depth_attachment_format;
        depth_attachment_format.format = depth_data_format;
        depth_attachment_format.samples = RenderingDeviceCommons::TEXTURE_SAMPLES_1;
        depth_attachment_format.usage_flags = RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

        Vector<RD::AttachmentFormat> attachments;
        attachments.push_back(color_attachment_format);
        attachments.push_back(depth_attachment_format);

        internal_fb_format = RD::get_singleton()->framebuffer_format_create(attachments);
        if (shared_stencil_buffer.is_valid()) {
            RD::get_singleton()->free_rid(shared_stencil_buffer);
            shared_stencil_buffer = RID();
        }
    }
    preprocess.current_preprocess_layer = 0;
}

void GodotRmlUiLayers::render_start() {
    DEV_ASSERT(layer_stack.is_empty());

    if (!shared_stencil_buffer.is_valid()) {
        recreate_shared_textures();
    }

    for (int i = 0; i < BACKBUFFER_COUNT; i++) {
        if (backbuffers[i].framebuffer.has_value()) {
            continue;
        }
        backbuffers[i].framebuffer = allocate_framebuffer(vformat("RmlUi Backbuffer %d", i));
    }

    for (int i = 0; i < allocated_layers.size(); i++) {
        Layer *layer = allocated_layers[i];
        if (layer->framebuffer.has_value()) {
            continue;
        }


        *layer = Layer {
            .framebuffer = allocate_framebuffer(vformat("Layer %d", i)),
            .idx = i
        };
    }
}

void GodotRmlUiLayers::push_layer() {
    RD *rd = RD::get_singleton();
    const int last_idx = layer_stack.size();
    layer_stack.push_back(allocated_layers[last_idx]);
    DEV_ASSERT(layer_stack[last_idx]->idx == last_idx);
    rd->texture_clear(layer_stack[last_idx]->framebuffer->color_texture, Color(0.0f, 0.0f, 0.0f, 0.0f), 0, 1, 0, 1);
}

GodotRmlUiLayers::Layer *GodotRmlUiLayers::prepush_layer() {
    preprocess.current_preprocess_layer++;
    if (preprocess.current_preprocess_layer >= allocated_layers.size()) {
        Layer *layer = memnew(Layer);
        layer->idx = allocated_layers.size();
        allocated_layers.push_back(layer);
    }

    preprocess.max_depth = MAX(preprocess.current_preprocess_layer+1, preprocess.max_depth);

    return allocated_layers[preprocess.current_preprocess_layer];
}

void GodotRmlUiLayers::prepop_layer() {
    preprocess.current_preprocess_layer--;
}

void GodotRmlUiLayers::pop_layer() {
    DEV_ASSERT(!layer_stack.is_empty());
    layer_stack.remove_at(layer_stack.size()-1);
}

Rml::LayerHandle GodotRmlUiLayers::get_layer(int p_layer) const {
    ERR_FAIL_INDEX_V(p_layer, layer_stack.size(), reinterpret_cast<Rml::LayerHandle>(nullptr));
    return reinterpret_cast<Rml::LayerHandle>(layer_stack[p_layer]);
}

RID GodotRmlUiLayers::get_layer_color_texture(int p_layer) const {
    ERR_FAIL_INDEX_V(p_layer, layer_stack.size(), RID());
    DEV_ASSERT(layer_stack[p_layer]->idx == p_layer);
    return layer_stack[p_layer]->framebuffer->color_texture;
}

RID GodotRmlUiLayers::get_layer_framebuffer(int p_layer) const {
    ERR_FAIL_INDEX_V(p_layer, layer_stack.size(), RID());
    return layer_stack[p_layer]->framebuffer->framebuffer;
}

int GodotRmlUiLayers::get_current_layer() const {
    DEV_ASSERT(!layer_stack.is_empty());
    return layer_stack.size()-1;
}

RID GodotRmlUiLayers::get_resolve_buffer() const {
    return resolve_intermediary_buffer;
}

int GodotRmlUiLayers::layer_get_idx(Rml::LayerHandle p_layer) const {
    Layer *layer = reinterpret_cast<Layer*>(p_layer);
    if (layer == nullptr) {
        layer = allocated_layers[0];
    }
    int idx = layer_stack.find(layer);
    ERR_FAIL_COND_V(idx == -1, -1);
    return idx;
}

RD::FramebufferFormatID GodotRmlUiLayers::get_framebuffer_format() const {
    return internal_fb_format;
}

GodotRmlUiLayers::AllocatedFramebuffer GodotRmlUiLayers::allocate_framebuffer(const String &p_hint_name) const {
    RD *rd = RD::get_singleton();
    const Vector2i size = rd->framebuffer_get_size(framebuffer);
    const RD::DataFormat color_data_format = RenderingDeviceCommons::DATA_FORMAT_R8G8B8A8_UNORM;

    RD::TextureFormat color_format = {
        .format = color_data_format,
        .width = static_cast<uint32_t>(size.x),
        .height = static_cast<uint32_t>(size.y),
        .usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_CAN_COPY_TO_BIT | RD::TEXTURE_USAGE_STORAGE_BIT | RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT
    };

    RD::TextureView texture_view = {};

    RID color_texture = rd->texture_create(color_format, texture_view);
    rd->set_resource_name(color_texture, vformat("%s framebuffer color texture", p_hint_name));

    DEV_ASSERT(color_texture.is_valid());

    RID fb = rd->framebuffer_create({ color_texture, shared_stencil_buffer }, internal_fb_format);
    
    rd->set_resource_name(fb, vformat("%s framebuffer", p_hint_name));

    return {
        .framebuffer = fb,
        .color_texture = color_texture
    };
}

RID GodotRmlUiLayers::backbuffer_get_framebuffer(int p_idx) const {
    ERR_FAIL_INDEX_V(p_idx, BACKBUFFER_COUNT, RID());
    return backbuffers[p_idx].framebuffer->framebuffer;
}

RID GodotRmlUiLayers::backbuffer_get_color_texture(int p_idx) const {
    ERR_FAIL_INDEX_V(p_idx, BACKBUFFER_COUNT, RID());
    return backbuffers[p_idx].framebuffer->color_texture;
}

GodotRmlUiLayers::~GodotRmlUiLayers() {
    clear_layers();
}
