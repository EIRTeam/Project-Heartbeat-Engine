#pragma once

#include "rmlui/backend/godot_rmlui_layers.h"
#include "rmlui/backend/godot_rmlui_shaders.h"
#include "rmlui/backend/shaders/color.glsl.gen.h"
#include "rmlui/backend/shaders/texture.glsl.gen.h"
#include "RmlUi/Core/RenderInterface.h"
#include "RmlUi/Core/Types.h"
#include "servers/rendering/rendering_device.h"
#include <optional>
#include <variant>

class RenderInterface_Godot_RD : public Rml::RenderInterface {
    struct GeometryView {
        RID vertex_buffer;
		RID vertex_array;
        RID index_buffer;
		RID index_array;
    };

	struct Texture {
		Ref<Texture2D> texture_ref;
	};

	RD::VertexFormatID get_vertex_format() const;

	// RmlUi interface
    virtual Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) override;
	virtual void RenderGeometry(Rml::CompiledGeometryHandle handle, Rml::Vector2f translation, Rml::TextureHandle texture) override;
    virtual void ReleaseGeometry(Rml::CompiledGeometryHandle p_geometry) override;
	virtual void SetTransform(const Rml::Matrix4f* transform) override;
	virtual Rml::LayerHandle PushLayer() override;
	virtual void PopLayer() override;
	virtual void ReleaseTexture(Rml::TextureHandle texture) override;
	virtual Rml::TextureHandle LoadTexture(Rml::Vector2i& r_texture_dimensions, const Rml::String& p_source) override;
	virtual Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i source_dimensions) override;
	virtual void EnableScissorRegion(bool enable) override;
	virtual void SetScissorRegion(Rml::Rectanglei region) override;
	virtual Rml::TextureHandle SaveLayerAsTexture() override;
	virtual void RenderToClipMask(Rml::ClipMaskOperation operation, Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation) override;
	virtual void EnableClipMask(bool enable) override;

	static constexpr int MAX_ELEMENT_TRANSFORMS = 128;

	RID context_data_buffer;
	RID element_transform_buffer;
	Vector2i current_fb_size;

	struct ContextUniform {
		float projection[16];
	};

	struct ElementTransform {
		Projection transform;
	};

	ElementTransform element_transforms[MAX_ELEMENT_TRANSFORMS];
	int element_transform_count = 0;

	RD::DrawListID draw_list = 0;

	struct PushConstantData {
		Vector2 translation;
		int transform_index = -1;
		uint8_t padding[4];
	} push_constant_data;
	bool push_constant_dirty = true;

	enum class BoundShader {
		COLOR,
		TEXTURE
	};

	struct RenderGeometryCommand {
		BoundShader shader;
		Rml::CompiledGeometryHandle geometry;
		Rml::TextureHandle texture;
		Rml::Vector2f translation;
	};

	struct SetTransformIndexCommand {
		int transform_idx;
	};

	struct ScissorEnableCommand {
		bool enabled;
	};

	struct ScissorSetRegionCommand {
		Rect2i region;
	};
	struct SaveLayerAsTextureCommand {
		Rml::TextureHandle out_texture;
	};

	struct PushLayerCommand {};
	struct PopLayerCommand {};
	struct RenderToClipMaskCommand {
		Rml::ClipMaskOperation operation;
		Rml::CompiledGeometryHandle geometry_handle;
		Vector2 translation;
	};

	struct SetClipMaskEnabledCommand {
		bool enabled;
	};

	using RenderCommand = std::variant<
		RenderGeometryCommand,
		SetTransformIndexCommand,
		ScissorEnableCommand,
		ScissorSetRegionCommand,
		PushLayerCommand,
		PopLayerCommand,
		SaveLayerAsTextureCommand,
		RenderToClipMaskCommand,
		SetClipMaskEnabledCommand
	>;

	std::vector<RenderCommand> render_commands;
	static constexpr int SHARED_UNIFORM_SET_IDX = 0;
	static constexpr int TEXTURE_SHADER_UNIFORM_SET_IDX = 1;
	std::optional<BoundShader> currently_bound_shader;
	GodotRmlUiShaders::RmluiPipelineID currently_bound_pipeline = 0;
	struct State {
		struct ScissorState {
			bool enabled = false;
			std::optional<Rect2i> region;
		} scissor_state;
		struct ClipMaskState {
			bool enabled = false;
			Rml::ClipMaskOperation operation;
			int stencil_test_value = 0;
			int stencil_write_value = 0;
		} clip_mask_state;
	} render_state;

	bool shaders_initialized = false;

	LocalVector<GeometryView *> geometry_deletion_queue;
	LocalVector<Texture *> texture_deletion_queue;

	GodotRmlUiLayers layers;
	GodotRmlUiShaders shaders;
	RD::VertexFormatID vertex_format;
	Rml::CompiledGeometryHandle fullscreen_quad_geometry = 0;
	
	void end_draw_pass();
	void begin_draw_pass(bool p_clear = false);

	void _ensure_in_draw_pass();
	void flush_element_transforms_buffer();
	void execute_command(const RenderGeometryCommand &p_command);
	void execute_command(const SetTransformIndexCommand &p_command);
	void execute_command(const ScissorEnableCommand &p_command);
	void execute_command(const ScissorSetRegionCommand &p_command);
	void execute_command(const PushLayerCommand &p_command);
	void execute_command(const PopLayerCommand &p_command);
	void execute_command(const SaveLayerAsTextureCommand &p_command);
	void execute_command(const RenderToClipMaskCommand &p_command);
	void execute_command(const SetClipMaskEnabledCommand &p_command);
	void render_geometry(const Rml::CompiledGeometryHandle p_geometry, const Vector2 &p_translation);
	bool is_in_draw_pass();
	void execute_blit(RID p_from, RID p_to, const Vector2i &p_source, const Vector2i &p_size, bool p_flip_y = true);
	void rebind_shared_uniforms(RID p_shader);
	void present();
	void flush_resource_deletion_queue();

	RID get_final_framebuffer() const;
	RID get_framebuffer() const;
	RD::FramebufferFormatID get_final_framebuffer_format() const;
	RD::FramebufferFormatID get_framebuffer_format() const;
public:
	void initialize();
	void begin_frame();
	void render();
    void end_frame();
	void set_projection(const Projection &p_projection);
	RenderInterface_Godot_RD();
    virtual ~RenderInterface_Godot_RD();
};