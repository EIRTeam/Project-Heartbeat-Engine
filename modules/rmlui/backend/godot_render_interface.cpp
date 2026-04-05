#include "core/error/error_macros.h"
#include "core/io/image.h"
#include "core/math/math_funcs.h"
#include "core/string/print_string.h"
#include "engine/core/io/resource_loader.h"
#include "godot_conversion.h"
#include "godot_render_interface.h"
#include "rmlui/backend/godot_rmlui_shaders.h"
#include "RmlUi/Core/Math.h"
#include "RmlUi/Core/Mesh.h"
#include "RmlUi/Core/MeshUtilities.h"
#include "RmlUi/Core/RenderInterface.h"
#include "RmlUi/Core/Types.h"
#include "RmlUi/Core/Vertex.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/texture_rd.h"
#include "servers/display/display_server.h"
#include "servers/rendering/renderer_rd/storage_rd/material_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/texture_storage.h"
#include "servers/rendering/renderer_rd/uniform_set_cache_rd.h"
#include "servers/rendering/rendering_device_commons.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/rendering_server.h"

Rml::CompiledGeometryHandle RenderInterface_Godot_RD::CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) {
    const uint32_t vertex_size = (vertices.size() * sizeof(Rml::Vertex));
    const uint32_t index_size = (indices.size() * sizeof(int));

    // Convert indices to uint32_t
    Vector<uint8_t> indices_converted;
    indices_converted.resize(index_size);
    uint32_t *indices_converted_ptrw = (uint32_t*)indices_converted.ptrw();
    for (size_t i = 0; i < indices.size(); i++) {
        indices_converted_ptrw[i] = indices[i];
    }

    RD *rd = RD::get_singleton();

    GeometryView *view = memnew(GeometryView);
    view->index_buffer = rd->index_buffer_create(indices.size(), RenderingDeviceCommons::INDEX_BUFFER_FORMAT_UINT32, indices_converted);
    
    Span<uint8_t> vtx_buffer_span = Span<uint8_t>((uint8_t*)vertices.data(), vertex_size);
    view->vertex_buffer = rd->vertex_buffer_create(vertex_size, vtx_buffer_span);

    view->vertex_array = rd->vertex_array_create(vertices.size(), vertex_format, Vector<RID>({view->vertex_buffer}));
    view->index_array = rd->index_array_create(view->index_buffer, 0, indices.size());

    return reinterpret_cast<Rml::CompiledGeometryHandle>(view);
}

void RenderInterface_Godot_RD::RenderGeometry(Rml::CompiledGeometryHandle p_geometry, Rml::Vector2f p_translation, Rml::TextureHandle p_texture) {
    render_commands.push_back({
        RenderGeometryCommand {
            .geometry = p_geometry,
            .texture = p_texture,
            .translation = p_translation
        }
    });
}

void RenderInterface_Godot_RD::ReleaseGeometry(Rml::CompiledGeometryHandle p_geometry) {
    GeometryView* view = reinterpret_cast<GeometryView*>(p_geometry);
    if (view != nullptr) {
        geometry_deletion_queue.push_back(view);
    }
}

void RenderInterface_Godot_RD::SetTransform(const Rml::Matrix4f* p_transform) {
    if (p_transform == nullptr) {
        // Set back to identity
        render_commands.push_back(SetTransformIndexCommand {
            .transform_idx = -1
        });
        return;
    }

	Projection proj;
    proj.columns[0].coord[0] = p_transform->GetColumn(0).x;
    proj.columns[0].coord[1] = p_transform->GetColumn(0).y;
    proj.columns[0].coord[2] = p_transform->GetColumn(0).z;
    proj.columns[0].coord[3] = p_transform->GetColumn(0).w;

    proj.columns[1].coord[0] = p_transform->GetColumn(1).x;
    proj.columns[1].coord[1] = p_transform->GetColumn(1).y;
    proj.columns[1].coord[2] = p_transform->GetColumn(1).z;
    proj.columns[1].coord[3] = p_transform->GetColumn(1).w;

    proj.columns[2].coord[0] = p_transform->GetColumn(2).x;
    proj.columns[2].coord[1] = p_transform->GetColumn(2).y;
    proj.columns[2].coord[2] = p_transform->GetColumn(2).z;
    proj.columns[2].coord[3] = p_transform->GetColumn(2).w;

    proj.columns[3].coord[0] = p_transform->GetColumn(3).x;
    proj.columns[3].coord[1] = p_transform->GetColumn(3).y;
    proj.columns[3].coord[2] = p_transform->GetColumn(3).z;
    proj.columns[3].coord[3] = p_transform->GetColumn(3).w;

    ERR_FAIL_COND_MSG(element_transform_count >= MAX_ELEMENT_TRANSFORMS-1, "RmlUi: Ran out of transforms in the transform buffer!");

    element_transforms[element_transform_count++].transform = proj;

    render_commands.push_back(SetTransformIndexCommand {
        .transform_idx = element_transform_count-1
    });
}

RD::FramebufferFormatID RenderInterface_Godot_RD::get_framebuffer_format() const {
	return layers.get_framebuffer_format();
}

RID RenderInterface_Godot_RD::get_final_framebuffer() const {
    RID viewport = SceneTree::get_singleton()->get_root()->get_viewport_rid();
    RID render_target = RS::get_singleton()->viewport_get_render_target(viewport);
    RID framebuffer = RendererRD::TextureStorage::get_singleton()->render_target_get_rd_framebuffer(render_target);
    return framebuffer;
}

RID RenderInterface_Godot_RD::get_framebuffer() const {
    return layers.get_layer_framebuffer(layers.get_current_layer());
}

RD::FramebufferFormatID RenderInterface_Godot_RD::get_final_framebuffer_format() const {
	return RD::get_singleton()->framebuffer_get_format(get_final_framebuffer());
}

void RenderInterface_Godot_RD::render() {
    RD *rd = RD::get_singleton();

    flush_element_transforms_buffer();
    layers.render_start();
    layers.push_layer();

    Vector2i fb_size = rd->framebuffer_get_size(get_final_framebuffer());
    if (current_fb_size != fb_size) {
        current_fb_size = fb_size;

        if (fullscreen_quad_geometry != 0) {
            ReleaseGeometry(fullscreen_quad_geometry);
            fullscreen_quad_geometry = 0;
        }

        Rml::Mesh mesh;
        Rml::MeshUtilities::GenerateQuad(mesh, Rml::Vector2f(0), Rml::Vector2f(current_fb_size.x, current_fb_size.y), Rml::ColourbPremultiplied(255, 255, 255));
        fullscreen_quad_geometry = CompileGeometry(mesh.vertices, mesh.indices);
    }

    rd->draw_command_begin_label("RmlUi", Color(0.0, 1.0,0.0));

    for (RenderCommand &command : render_commands) {
        std::visit([this](const auto &p_command) {
            execute_command(p_command);
        }, command);
    }
    end_draw_pass();
    present();
    layers.pop_layer();
    rd->draw_command_end_label();
}

void RenderInterface_Godot_RD::initialize() {
    context_data_buffer = RD::get_singleton()->storage_buffer_create(sizeof(ContextUniform));
    element_transform_buffer = RD::get_singleton()->storage_buffer_create(sizeof(ElementTransform) * MAX_ELEMENT_TRANSFORMS);
    vertex_format = get_vertex_format();

    layers.init();
}

void RenderInterface_Godot_RD::begin_frame() {
    flush_resource_deletion_queue();
    element_transform_count = 0;
    push_constant_data.transform_index = -1;
    currently_bound_shader.reset();
    render_commands.clear();

    layers.frame_start(get_final_framebuffer());

    if (!shaders_initialized) {
        shaders.initialize(get_framebuffer_format(), vertex_format);
        shaders_initialized = true;
    }

    render_state.clip_mask_state.stencil_test_value = 0;
    render_state.clip_mask_state.stencil_write_value = 0;
}

void RenderInterface_Godot_RD::end_frame() {}

Rml::LayerHandle RenderInterface_Godot_RD::PushLayer() {
	render_commands.push_back(PushLayerCommand());
    return reinterpret_cast<Rml::LayerHandle>(layers.prepush_layer());
}

void RenderInterface_Godot_RD::PopLayer() {
	render_commands.push_back(PopLayerCommand());
    layers.prepop_layer();
}

RD::VertexFormatID RenderInterface_Godot_RD::get_vertex_format() const {
    RD::VertexAttribute attrib_pos = {
        .binding = 0,
        .location = 0,
        .offset = offsetof(Rml::Vertex, position),
        .format = RD::DATA_FORMAT_R32G32_SFLOAT,
        .stride = sizeof(Rml::Vertex)
    };

    RD::VertexAttribute attrib_color = {
        .binding = 0,
        .location = 1,
        .offset = offsetof(Rml::Vertex, colour),
        .format = RD::DATA_FORMAT_R8G8B8A8_UNORM,
        .stride = sizeof(Rml::Vertex)
    };

    RD::VertexAttribute attrib_tex_coord = {
        .binding = 0,
        .location = 2,
        .offset = offsetof(Rml::Vertex, tex_coord),
        .format = RD::DATA_FORMAT_R32G32_SFLOAT,
        .stride = sizeof(Rml::Vertex)
    };

    Vector<RD::VertexAttribute> attribs;
    attribs.push_back(attrib_pos);
    attribs.push_back(attrib_color);
    attribs.push_back(attrib_tex_coord);

    RD::VertexFormatID vtx_format = RD::get_singleton()->vertex_format_create(attribs);
    return vtx_format;
}

Rml::TextureHandle RenderInterface_Godot_RD::LoadTexture(Rml::Vector2i& r_texture_dimensions, const Rml::String& p_source) {
	Ref<Texture2D> texture = ResourceLoader::load(to_godot(p_source));

    if (!texture.is_valid()) {
        r_texture_dimensions = Rml::Vector2i();
        return reinterpret_cast<Rml::TextureHandle>(nullptr);
    }

    Texture *tex = memnew(Texture);
    tex->texture_ref = texture;

    RD::get_singleton()->set_resource_name(RS::get_singleton()->texture_get_rd_texture(texture->get_rid()), p_source.c_str());

    const Vector2i size = texture->get_size();

    r_texture_dimensions = Rml::Vector2i(
        size.x,
        size.y
    );

    return reinterpret_cast<Rml::TextureHandle>(tex);
}

Rml::TextureHandle RenderInterface_Godot_RD::GenerateTexture(Rml::Span<const Rml::byte> p_source, Rml::Vector2i p_source_dimensions) {
	
    Vector<uint8_t> stupid_godot;
    stupid_godot.resize(p_source.size());

    uint8_t *stupid_write = stupid_godot.ptrw();

    std::memcpy(stupid_write, p_source.data(), p_source.size());

    Ref<Image> img = Image::create_from_data(p_source_dimensions.x, p_source_dimensions.y, false, Image::FORMAT_RGBA8, stupid_godot);

    Ref<ImageTexture> tex = ImageTexture::create_from_image(img);

    Texture *view = memnew(Texture);
    view->texture_ref = tex;

    return reinterpret_cast<Rml::TextureHandle>(view);
}

void RenderInterface_Godot_RD::ReleaseTexture(Rml::TextureHandle p_texture) {
	Texture *tex = reinterpret_cast<Texture*>(p_texture);

    if (tex == nullptr) {
        return;
    }

    texture_deletion_queue.push_back(tex);
}

void RenderInterface_Godot_RD::EnableScissorRegion(bool p_enable) {
    render_commands.push_back(ScissorEnableCommand {
        .enabled = p_enable
    });
}

void RenderInterface_Godot_RD::SetScissorRegion(Rml::Rectanglei region) {
    Rml::Vector2 top_left = region.TopLeft();
    Rml::Vector2 size = region.Size();
    render_commands.push_back(ScissorSetRegionCommand {
        .region = Rect2(top_left.x, top_left.y, size.x, size.y)
    });
}

Rml::TextureHandle RenderInterface_Godot_RD::SaveLayerAsTexture() {
	Rml::TextureHandle out_texture = reinterpret_cast<Rml::TextureHandle>(memnew(Texture));
    render_commands.push_back(SaveLayerAsTextureCommand {
        .out_texture = out_texture
    });
    return out_texture;
}

void RenderInterface_Godot_RD::RenderToClipMask(Rml::ClipMaskOperation operation, Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation) {
	render_commands.push_back(RenderToClipMaskCommand {
        .operation = operation,
        .geometry_handle = geometry,
        .translation = Vector2(translation.x, translation.y)
    });
}

void RenderInterface_Godot_RD::flush_element_transforms_buffer() {
	if (element_transform_count > 0) {
        RD::get_singleton()->buffer_update(element_transform_buffer, 0, element_transform_count * sizeof(ElementTransform), element_transforms);
    }
}

void RenderInterface_Godot_RD::execute_command(const RenderGeometryCommand &p_command) {
    _ensure_in_draw_pass();
    RD *rd = RD::get_singleton();
    Texture *texture = reinterpret_cast<Texture*>(p_command.texture);

    UniformSetCacheRD *uniform_cache = UniformSetCacheRD::get_singleton();

    BoundShader new_shader;
    GodotRmlUiShaders::RmluiPipelineID new_pipeline_id;
    GodotRmlUiShaders::RenderShaderVariant new_variant = render_state.clip_mask_state.enabled ? GodotRmlUiShaders::SHADER_VARIANT_STENCIL_COMPARE : GodotRmlUiShaders::SHADER_VARIANT_NORMAL;

    if (texture != nullptr) {
        new_shader = BoundShader::TEXTURE;
        new_pipeline_id = shaders.get_pipeline_id(GodotRmlUiShaders::RENDER_SHADER_TEXTURE, new_variant, get_framebuffer_format(), get_vertex_format());
    } else {
        new_shader = BoundShader::COLOR;
        new_pipeline_id = shaders.get_pipeline_id(GodotRmlUiShaders::RENDER_SHADER_COLOR, new_variant, get_framebuffer_format(), get_vertex_format());
    }

    if (!currently_bound_shader.has_value() || *currently_bound_shader != new_shader || new_pipeline_id != currently_bound_pipeline) {
        RID shader;
        currently_bound_shader = new_shader;
        currently_bound_pipeline = new_pipeline_id;
        switch(*currently_bound_shader) {
			case BoundShader::COLOR: {
                rd->draw_list_bind_render_pipeline(draw_list, shaders.get_render_pipeline(
                    GodotRmlUiShaders::RENDER_SHADER_COLOR, new_variant, get_framebuffer_format(), get_vertex_format()));
                shader = shaders.get_render_shader(GodotRmlUiShaders::RENDER_SHADER_COLOR);
            } break;
			case BoundShader::TEXTURE: {
                rd->draw_list_bind_render_pipeline(draw_list, shaders.get_render_pipeline(
                    GodotRmlUiShaders::RENDER_SHADER_TEXTURE, new_variant, get_framebuffer_format(), get_vertex_format()));
                shader = shaders.get_render_shader(GodotRmlUiShaders::RENDER_SHADER_TEXTURE);
            } break;
		}

        rebind_shared_uniforms(shader);
        push_constant_dirty = true;
        if (render_state.clip_mask_state.enabled) {
            rd->draw_list_set_stencil_reference(draw_list, render_state.clip_mask_state.stencil_test_value);
        }
	}

    if (*currently_bound_shader == BoundShader::TEXTURE) {
        RID sampler = RendererRD::MaterialStorage::get_singleton()->sampler_rd_get_default(RenderingServer::CANVAS_ITEM_TEXTURE_FILTER_LINEAR, RenderingServer::CANVAS_ITEM_TEXTURE_REPEAT_ENABLED);
        RID texture_rid = RS::get_singleton()->texture_get_rd_texture(texture->texture_ref->get_rid());
        RD::Uniform texture_uniform = RD::Uniform(RenderingDeviceCommons::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ sampler, texture_rid}));
        RID uniform_set = uniform_cache->get_cache(shaders.get_render_shader(GodotRmlUiShaders::RENDER_SHADER_TEXTURE), TEXTURE_SHADER_UNIFORM_SET_IDX, texture_uniform); 
        rd->draw_list_bind_uniform_set(draw_list, uniform_set, TEXTURE_SHADER_UNIFORM_SET_IDX);
    }

    render_geometry(p_command.geometry, Vector2(p_command.translation.x, p_command.translation.y));
}

void RenderInterface_Godot_RD::execute_command(const SetTransformIndexCommand &p_command) {
	push_constant_data.transform_index = p_command.transform_idx;
    push_constant_dirty = true;
}

void RenderInterface_Godot_RD::execute_command(const ScissorEnableCommand &p_command) {
    if (!p_command.enabled && is_in_draw_pass()) {
        RD::get_singleton()->draw_list_disable_scissor(draw_list);
    }

	render_state.scissor_state.enabled = p_command.enabled;
}

void RenderInterface_Godot_RD::execute_command(const ScissorSetRegionCommand &p_command) {
	render_state.scissor_state.region = p_command.region;
    if (is_in_draw_pass()) {
        if (!render_state.scissor_state.enabled && render_state.scissor_state.region.has_value()) {
            RD::get_singleton()->draw_list_enable_scissor(draw_list, *render_state.scissor_state.region);
        }
    }
}

void RenderInterface_Godot_RD::execute_command(const PushLayerCommand &p_command) {
    end_draw_pass();
    layers.push_layer();
}

void RenderInterface_Godot_RD::execute_command(const PopLayerCommand &p_command) {
	layers.pop_layer();
    end_draw_pass();
}

void RenderInterface_Godot_RD::execute_command(const SaveLayerAsTextureCommand &p_command) {
	end_draw_pass();
    int layer = layers.get_current_layer();
    DEV_ASSERT(layer != -1);
    RID to_save = layers.get_layer_color_texture(layer);
    RID blit_source = to_save;

    RD *rd = RD::get_singleton();
    if (rd->texture_get_format(to_save).samples != RenderingDeviceCommons::TEXTURE_SAMPLES_1) {
        RID resolve_buffer = layers.get_resolve_buffer();
        Error result = rd->texture_resolve_multisample(to_save, resolve_buffer);
        DEV_ASSERT(result == OK);
        blit_source = resolve_buffer;
    }

    const Rect2i base_fb_size = Rect2i(Vector2i(), rd->framebuffer_get_size(get_framebuffer()));
    Rect2i scissor_region = render_state.scissor_state.enabled ? render_state.scissor_state.region.value_or(base_fb_size) : base_fb_size;
    RD::TextureView texture_view = {};
    const RD::DataFormat color_data_format = RenderingDeviceCommons::DATA_FORMAT_R8G8B8A8_UNORM;
    
    RD::TextureFormat resolve_format = {
        .format = color_data_format,
        .width = static_cast<uint32_t>(scissor_region.size.x),
        .height = static_cast<uint32_t>(scissor_region.size.y),
        .usage_bits = RD::TEXTURE_USAGE_CAN_COPY_TO_BIT | RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_STORAGE_BIT
    };
    
    RID out_texture = rd->texture_create(resolve_format, texture_view);
    rd->draw_command_begin_label(vformat("Blit layer %d -> SavedTexture (%d x %d)", layers.get_current_layer(), scissor_region.size.x, scissor_region.size.y).utf8(), Color(1.0, 1.0, 0.0));
    execute_blit(blit_source, out_texture, scissor_region.position, scissor_region.size);
    rd->draw_command_end_label();
    Ref<Texture2DRD> tex;
    tex.instantiate();
    tex->set_texture_rd_rid(out_texture);
    reinterpret_cast<Texture*>(p_command.out_texture)->texture_ref = tex;
    rd->set_resource_name(out_texture, "SaveLayer");
}

void RenderInterface_Godot_RD::execute_command(const RenderToClipMaskCommand &p_command) {
    _ensure_in_draw_pass();
    RD *rd = RD::get_singleton();

    GodotRmlUiShaders::RenderShaderVariant render_shader_variant;

    switch (p_command.operation) {
		case Rml::ClipMaskOperation::Set: {
            render_shader_variant = GodotRmlUiShaders::SHADER_VARIANT_STENCIL_REPLACE;
            render_state.clip_mask_state.stencil_write_value = ++render_state.clip_mask_state.stencil_test_value;
        } break;
		case Rml::ClipMaskOperation::SetInverse: {
            render_state.clip_mask_state.stencil_test_value = 1;
            render_state.clip_mask_state.stencil_write_value = 0;
            render_shader_variant = GodotRmlUiShaders::SHADER_VARIANT_STENCIL_REPLACE;
        } break;
		case Rml::ClipMaskOperation::Intersect:
            render_shader_variant = GodotRmlUiShaders::SHADER_VARIANT_STENCIL_INCREASE;
            render_state.clip_mask_state.stencil_write_value = 1;
            render_state.clip_mask_state.stencil_test_value++;
			break;
	}

    currently_bound_pipeline = shaders.get_pipeline_id(GodotRmlUiShaders::RENDER_SHADER_COLOR, render_shader_variant, get_framebuffer_format(), vertex_format);
    currently_bound_shader = BoundShader::COLOR;

	rd->draw_list_bind_render_pipeline(draw_list, shaders.get_render_pipeline(GodotRmlUiShaders::RENDER_SHADER_COLOR, render_shader_variant, get_framebuffer_format(), vertex_format));
    push_constant_data.transform_index = -1;
    push_constant_dirty = true;
    rebind_shared_uniforms(shaders.get_render_shader(GodotRmlUiShaders::RENDER_SHADER_COLOR));

    if (p_command.operation == Rml::ClipMaskOperation::SetInverse) {
        rd->draw_list_set_stencil_reference(draw_list, 1);
        render_geometry(fullscreen_quad_geometry, Vector2());
    }
    rd->draw_list_set_stencil_reference(draw_list, render_state.clip_mask_state.stencil_write_value);
    render_geometry(p_command.geometry_handle, p_command.translation);
}

void RenderInterface_Godot_RD::execute_command(const SetClipMaskEnabledCommand &p_command) {
	render_state.clip_mask_state.enabled = p_command.enabled;
}

void RenderInterface_Godot_RD::render_geometry(const Rml::CompiledGeometryHandle p_geometry, const Vector2 &p_translation) {
    RD *rd = RD::get_singleton();
    push_constant_dirty = push_constant_dirty || (p_translation != push_constant_data.translation);

    if (push_constant_dirty) {
        push_constant_data.translation = p_translation;
        rd->draw_list_set_push_constant(draw_list, &push_constant_data, sizeof(push_constant_data));
        push_constant_dirty = false;
    }
    GeometryView *geometry = reinterpret_cast<GeometryView*>(p_geometry);
    rd->draw_list_bind_vertex_array(draw_list, geometry->vertex_array);
    rd->draw_list_bind_index_array(draw_list, geometry->index_array);
    rd->draw_list_draw(draw_list, true);
}

bool RenderInterface_Godot_RD::is_in_draw_pass() {
	return draw_list != 0;
}

void RenderInterface_Godot_RD::execute_blit(RID p_from, RID p_to, const Vector2i &p_source, const Vector2i &p_size, bool p_flip_y) {
    RD *rd = RD::get_singleton();
    RD::ComputeListID compute_list = rd->compute_list_begin();
    rd->compute_list_bind_compute_pipeline(compute_list, shaders.get_compute_pipeline(GodotRmlUiShaders::COMPUTE_SHADER_BLIT));
    
    RD::Uniform u_src(
        RenderingDeviceCommons::UNIFORM_TYPE_IMAGE,
        0,
        p_from
    );
    RD::Uniform u_dest(
        RenderingDeviceCommons::UNIFORM_TYPE_IMAGE,
        1,
        p_to
    );

    RID uniform_set = UniformSetCacheRD::get_singleton()->get_cache(shaders.get_compute_shader(GodotRmlUiShaders::COMPUTE_SHADER_BLIT), 0, u_src, u_dest);

    struct BlitPushConstant {
        Vector2i src;
        Vector2i dst;
        Vector2i copy_size;
        uint8_t flip_vertical;
        uint8_t padding[4];
    } push_c;

    push_c = {
        .src = p_source,
        .dst = Vector2i(),
        .copy_size = p_size,
        .flip_vertical = 1
    };


    rd->compute_list_bind_uniform_set(compute_list, uniform_set, 0);
    rd->compute_list_set_push_constant(compute_list, &push_c, sizeof(BlitPushConstant));

    rd->compute_list_dispatch(compute_list, Math::division_round_up(p_size.x, 8), Math::division_round_up(p_size.y, 8), 1);
    rd->compute_list_end();
}

void RenderInterface_Godot_RD::end_draw_pass() {
	RD *rd = RD::get_singleton();
	if (draw_list != 0) {
        draw_list = 0;
        rd->draw_list_end();
        rd->draw_command_end_label();
    }
    currently_bound_shader.reset();
    currently_bound_pipeline = 0;
}

void RenderInterface_Godot_RD::begin_draw_pass(bool p_clear) {
    DEV_ASSERT(draw_list == 0);
    RD *rd = RD::get_singleton();
    const int top_layer = layers.get_current_layer();
    const String layer_name = vformat("RmlUi Layer %d", top_layer);
    rd->draw_command_begin_label(layer_name.utf8(), Color(1.0, 0.0,0.0));
    draw_list = rd->draw_list_begin(get_framebuffer(), p_clear ? RenderingDevice::DRAW_CLEAR_COLOR_0 | RenderingDevice::DRAW_CLEAR_DEPTH : RenderingDevice::DRAW_DEFAULT_ALL);
    if (render_state.scissor_state.enabled && render_state.scissor_state.region.has_value()) {
        rd->draw_list_enable_scissor(draw_list, Rect2(*render_state.scissor_state.region));
    }
}

void RenderInterface_Godot_RD::_ensure_in_draw_pass() {
	if (draw_list == 0) {
        begin_draw_pass();
    }
}

void RenderInterface_Godot_RD::set_projection(const Projection &p_projection) {
    float projection[16];
	RendererRD::MaterialStorage::store_camera(p_projection, projection);
    RD::get_singleton()->buffer_update(context_data_buffer, 0, sizeof(projection), projection);
}

void RenderInterface_Godot_RD::rebind_shared_uniforms(RID p_shader) {
    RD *rd = RD::get_singleton();
    UniformSetCacheRD *uniform_cache = UniformSetCacheRD::get_singleton();
    RD::Uniform context_data_uniform = RD::Uniform(RenderingDeviceCommons::UNIFORM_TYPE_STORAGE_BUFFER, 0, context_data_buffer);
    RD::Uniform element_transform_uniform = RD::Uniform(RenderingDeviceCommons::UNIFORM_TYPE_STORAGE_BUFFER, 1, element_transform_buffer);
    rd->draw_list_bind_uniform_set(draw_list, uniform_cache->get_cache(p_shader, SHARED_UNIFORM_SET_IDX, context_data_uniform, element_transform_uniform), SHARED_UNIFORM_SET_IDX);
}

void RenderInterface_Godot_RD::EnableClipMask(bool enable) {
    render_commands.push_back(SetClipMaskEnabledCommand {
        .enabled = enable
    });
}

void RenderInterface_Godot_RD::present() {
	RD *rd = RD::get_singleton();
    rd->draw_command_begin_label("Present", Color(0.0, 1.0, 0.0));
    draw_list = rd->draw_list_begin(get_final_framebuffer());
    rd->draw_list_bind_render_pipeline(draw_list, shaders.get_render_pipeline(GodotRmlUiShaders::RENDER_SHADER_TEXTURE, GodotRmlUiShaders::SHADER_VARIANT_NORMAL, get_final_framebuffer_format(), vertex_format));
    
    rebind_shared_uniforms(shaders.get_render_shader(GodotRmlUiShaders::RENDER_SHADER_TEXTURE));
    push_constant_data.transform_index = -1;
    push_constant_data.translation = Vector2();
    push_constant_dirty = true;

    RID sampler = RendererRD::MaterialStorage::get_singleton()->sampler_rd_get_default(RenderingServer::CANVAS_ITEM_TEXTURE_FILTER_LINEAR, RenderingServer::CANVAS_ITEM_TEXTURE_REPEAT_ENABLED);
    RD::Uniform texture_uniform = RD::Uniform(RenderingDeviceCommons::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ sampler, layers.get_layer_color_texture(0)}));
    UniformSetCacheRD *uniform_cache = UniformSetCacheRD::get_singleton();
    RID uniform_set = uniform_cache->get_cache(shaders.get_render_shader(GodotRmlUiShaders::RENDER_SHADER_TEXTURE), TEXTURE_SHADER_UNIFORM_SET_IDX, texture_uniform); 
    rd->draw_list_bind_uniform_set(draw_list, uniform_set, TEXTURE_SHADER_UNIFORM_SET_IDX);
    render_geometry(fullscreen_quad_geometry, Vector2());
    rd->draw_list_end();
    rd->draw_command_end_label();
    draw_list = 0;
}

RenderInterface_Godot_RD::RenderInterface_Godot_RD() {
	
}

RenderInterface_Godot_RD::~RenderInterface_Godot_RD() {
    RD *rd = RD::get_singleton();

    if (element_transform_buffer.is_valid()) {
        rd->free_rid(element_transform_buffer);
    }

    if (context_data_buffer.is_valid()) {
        rd->free_rid(context_data_buffer);
    }

    if (fullscreen_quad_geometry != 0) {
        ReleaseGeometry(fullscreen_quad_geometry);
    }

    flush_resource_deletion_queue();
}

void RenderInterface_Godot_RD::flush_resource_deletion_queue() {
    RD *rd = RD::get_singleton();
    for(Texture *texture : texture_deletion_queue) {
        if (Ref<Texture2DRD> rdtx = texture->texture_ref; rdtx.is_valid()) {
            rd->free_rid(rdtx->get_texture_rd_rid());
        }
        memdelete(texture);
    }

    for (GeometryView *view : geometry_deletion_queue) {
        rd->free_rid(view->index_array);
        rd->free_rid(view->vertex_array);
        rd->free_rid(view->index_buffer);
        rd->free_rid(view->vertex_buffer);
        memdelete(view);
    }

    geometry_deletion_queue.clear();
    texture_deletion_queue.clear();
}
