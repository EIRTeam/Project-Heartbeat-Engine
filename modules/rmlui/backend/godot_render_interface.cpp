#include "godot_render_interface.h"
#include "RmlUi/Core/Math.h"
#include "RmlUi/Core/Types.h"
#include "RmlUi/Core/Vertex.h"
#include "core/error/error_macros.h"
#include "core/io/image.h"
#include "core/string/print_string.h"
#include "scene/main/window.h"
#include "scene/resources/image_texture.h"
#include "servers/display/display_server.h"
#include "servers/rendering/renderer_rd/storage_rd/material_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/texture_storage.h"
#include "servers/rendering/renderer_rd/uniform_set_cache_rd.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/rendering_device_commons.h"
#include "scene/main/scene_tree.h"
#include "servers/rendering/rendering_server.h"
#include "engine/core/io/resource_loader.h"
#include "godot_conversion.h"

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

    view->vertex_array = rd->vertex_array_create(vertices.size(), get_vertex_format(), Vector<RID>({view->vertex_buffer}));
    view->index_array = rd->index_array_create(view->index_buffer, 0, indices.size());

    return reinterpret_cast<Rml::CompiledGeometryHandle>(view);
}

void RenderInterface_Godot_RD::RenderGeometry(Rml::CompiledGeometryHandle p_geometry, Rml::Vector2f p_translation, Rml::TextureHandle p_texture) {
	RD *rd = RD::get_singleton();

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

void RenderInterface_Godot_RD::render() {
    RD *rd = RD::get_singleton();
    RID viewport = SceneTree::get_singleton()->get_root()->get_viewport_rid();
    RID render_target = RS::get_singleton()->viewport_get_render_target(viewport);
    RID framebuffer = RendererRD::TextureStorage::get_singleton()->render_target_get_rd_framebuffer(render_target);
    flush_element_transforms_buffer();

    rd->draw_command_begin_label("RmlUi", Color(1.0, 0.0,0.0));

    draw_list = rd->draw_list_begin(framebuffer, RenderingDevice::DRAW_DEFAULT_ALL);
    for (RenderCommand &command : render_commands) {
        std::visit([this](const auto &p_command) {
            execute_command(p_command);
        }, command);
    }

    rd->draw_list_end();
    rd->draw_command_end_label();
    draw_list = 0;
}

void RenderInterface_Godot_RD::Initialize() {
	create_pipelines();
    context_data_buffer = RD::get_singleton()->storage_buffer_create(sizeof(ContextUniform));
    element_transform_buffer = RD::get_singleton()->storage_buffer_create(sizeof(ElementTransform) * MAX_ELEMENT_TRANSFORMS);
}

void RenderInterface_Godot_RD::BeginFrame() {
    for (Texture *tex : texture_deletion_queue) {
        memfree(tex);
    }

    texture_deletion_queue.clear();
    RD *rd = RD::get_singleton();

    for (GeometryView *view : geometry_deletion_queue) {
        rd->free_rid(view->index_array);
        rd->free_rid(view->vertex_array);
        rd->free_rid(view->index_buffer);
        rd->free_rid(view->vertex_buffer);
        memdelete(view);
    }
    geometry_deletion_queue.clear();

    element_transform_count = 0;
    push_constant_data.transform_index = -1;
    currently_bound_shader.reset();
    render_commands.clear();
}

void RenderInterface_Godot_RD::EndFrame() {}

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

void RenderInterface_Godot_RD::create_pipelines() {
    Vector<String> variants;
    variants.push_back("");
    pipelines.texture_shader.initialize(variants);
    pipelines.texture_shader_version = pipelines.texture_shader.version_create();
    pipelines.texture_shader_rd = pipelines.texture_shader.version_get_shader(pipelines.texture_shader_version, 0);
    
    pipelines.color_shader.initialize(variants);
    pipelines.color_shader_version = pipelines.color_shader.version_create();
    pipelines.color_shader_rd = pipelines.color_shader.version_get_shader(pipelines.color_shader_version, 0);

    RD::FramebufferFormatID fb_format = RD::get_singleton()->screen_get_framebuffer_format();

    RD::PipelineRasterizationState rasterization_state = RD::PipelineRasterizationState();
    RD::PipelineMultisampleState multisample_state = RD::PipelineMultisampleState();
    RD::PipelineColorBlendState blend_state = RD::PipelineColorBlendState::create_blend(1);
    
    RD::PipelineDepthStencilState depth_stencil;
    depth_stencil.enable_stencil = true;

    pipelines.color_pipeline = RD::get_singleton()->render_pipeline_create(pipelines.color_shader_rd, fb_format, get_vertex_format(), RD::RENDER_PRIMITIVE_TRIANGLES, rasterization_state, multisample_state, depth_stencil, blend_state);
    pipelines.texture_pipeline = RD::get_singleton()->render_pipeline_create(pipelines.texture_shader_rd, fb_format, get_vertex_format(), RD::RENDER_PRIMITIVE_TRIANGLES, rasterization_state, multisample_state, depth_stencil, blend_state);
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

void RenderInterface_Godot_RD::flush_element_transforms_buffer() {
	if (element_transform_count > 0) {
        RD::get_singleton()->buffer_update(element_transform_buffer, 0, element_transform_count * sizeof(ElementTransform), element_transforms);
    }
}

void RenderInterface_Godot_RD::execute_command(const RenderGeometryCommand &p_command) {
    RD *rd = RD::get_singleton();
    Texture *texture = reinterpret_cast<Texture*>(p_command.texture);
    GeometryView *geometry = reinterpret_cast<GeometryView*>(p_command.geometry);

    UniformSetCacheRD *uniform_cache = UniformSetCacheRD::get_singleton();

    BoundShader new_shader;

    if (texture != nullptr) {
        new_shader = BoundShader::TEXTURE;
        rd->draw_list_bind_render_pipeline(draw_list, pipelines.texture_pipeline);
    } else {
        new_shader = BoundShader::COLOR;
        rd->draw_list_bind_render_pipeline(draw_list, pipelines.color_pipeline);
    }

    if (!currently_bound_shader.has_value() || *currently_bound_shader != new_shader) {
        RID shader;
        currently_bound_shader = new_shader;
        switch(*currently_bound_shader) {
			case BoundShader::COLOR: {
                rd->draw_list_bind_render_pipeline(draw_list, pipelines.color_pipeline);
                shader = pipelines.color_shader_rd;
            } break;
			case BoundShader::TEXTURE: {
                rd->draw_list_bind_render_pipeline(draw_list, pipelines.texture_pipeline);
                shader = pipelines.texture_shader_rd;
            } break;
		}

        rebind_shared_uniforms(shader);
        push_constant_dirty = true;
	}

    if (*currently_bound_shader == BoundShader::TEXTURE) {
        RID sampler = RendererRD::MaterialStorage::get_singleton()->sampler_rd_get_default(RenderingServer::CANVAS_ITEM_TEXTURE_FILTER_LINEAR, RenderingServer::CANVAS_ITEM_TEXTURE_REPEAT_ENABLED);
        RID texture_rid = RS::get_singleton()->texture_get_rd_texture(texture->texture_ref->get_rid());
        RD::Uniform texture_uniform = RD::Uniform(RenderingDeviceCommons::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ sampler, texture_rid}));
        RID uniform_set = uniform_cache->get_cache(pipelines.texture_shader_rd, TEXTURE_SHADER_UNIFORM_SET_IDX, texture_uniform); 
        rd->draw_list_bind_uniform_set(draw_list, uniform_set, TEXTURE_SHADER_UNIFORM_SET_IDX);
    }

    Vector2 translation = Vector2(p_command.translation.x, p_command.translation.y);

    push_constant_dirty = push_constant_dirty || (translation != push_constant_data.translation);

    if (push_constant_dirty) {
        push_constant_data.translation = translation;
        rd->draw_list_set_push_constant(draw_list, &push_constant_data, sizeof(push_constant_data));
        push_constant_dirty = false;
    }
    rd->draw_list_bind_vertex_array(draw_list, geometry->vertex_array);
    rd->draw_list_bind_index_array(draw_list, geometry->index_array);
    rd->draw_list_draw(draw_list, true);
}

void RenderInterface_Godot_RD::execute_command(const SetTransformIndexCommand &p_command) {
	push_constant_data.transform_index = p_command.transform_idx;
    push_constant_dirty = true;
}

void RenderInterface_Godot_RD::execute_command(const ScissorEnableCommand &p_command) {
    if (!scissor_state.enabled && p_command.enabled && scissor_state.region.has_value()) {
        RD::get_singleton()->draw_list_enable_scissor(draw_list, *scissor_state.region);
    } else if (scissor_state.enabled && !p_command.enabled) {
        RD::get_singleton()->draw_list_disable_scissor(draw_list);
    }
	scissor_state.enabled = p_command.enabled;
}

void RenderInterface_Godot_RD::execute_command(const ScissorSetRegionCommand &p_command) {
	scissor_state.region = p_command.region;
    if (scissor_state.enabled && scissor_state.region.has_value()) {
        RD::get_singleton()->draw_list_enable_scissor(draw_list, *scissor_state.region);
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

RenderInterface_Godot_RD::RenderInterface_Godot_RD() {
	
}

RenderInterface_Godot_RD::~RenderInterface_Godot_RD() {
    RD *rd = RD::get_singleton();

    if (context_data_buffer.is_valid()) {
        rd->free_rid(context_data_buffer);
    }

    if (pipelines.texture_pipeline.is_valid()) {
        rd->free_rid(pipelines.texture_pipeline);
    }

    if (pipelines.color_pipeline.is_valid()) {
        rd->free_rid(pipelines.color_pipeline);
    }

    pipelines.texture_shader.version_free(pipelines.texture_shader_version);
    pipelines.color_shader.version_free(pipelines.color_shader_version);
}
