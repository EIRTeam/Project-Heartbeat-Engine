#pragma once

#include "RmlUi/Core/RenderInterface.h"
#include "RmlUi/Core/Types.h"
#include "rmlui/backend/shaders/color.glsl.gen.h"
#include "rmlui/backend/shaders/texture.glsl.gen.h"
#include "servers/rendering/rendering_device.h"
#include <optional>
#include <variant>

class RenderInterface_Godot_RD : public Rml::RenderInterface {

    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) override;
	void RenderGeometry(Rml::CompiledGeometryHandle handle, Rml::Vector2f translation, Rml::TextureHandle texture) override;
    void ReleaseGeometry(Rml::CompiledGeometryHandle p_geometry) override;
	void SetTransform(const Rml::Matrix4f* transform) override;
public:
	void Initialize();
	void BeginFrame();
	void render();
    void EndFrame();
private:
    friend struct EnableScissorRegionCommand;
	friend struct SetScissorRegionCommand;
	friend struct RenderGeometryCommand;
	friend struct ReleaseGeometryCommand;
	friend struct ReleaseTextureCommand;
	friend struct SetTransformCommand;

    struct GeometryView {
        RID vertex_buffer;
		RID vertex_array;
        RID index_buffer;
		RID index_array;
    };

	struct Texture {
		Ref<Texture2D> texture_ref;
	};

    struct {
        RID texture_pipeline;
        RID color_pipeline;

        ColorShaderRD color_shader;
		RID color_shader_version;
        RID color_shader_rd;
        
		TextureShaderRD texture_shader;
		RID texture_shader_version;
        RID texture_shader_rd;
    } pipelines;

	RD::VertexFormatID get_vertex_format() const;

	Rml::TextureHandle LoadTexture(Rml::Vector2i& r_texture_dimensions, const Rml::String& p_source) override;
	Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i source_dimensions) override;
	/// Called by RmlUi when a loaded or generated texture is no longer required.
	/// @param[in] texture The texture handle to release.
	void ReleaseTexture(Rml::TextureHandle texture) override;

    void create_pipelines();

	void EnableScissorRegion(bool enable) override;
	void SetScissorRegion(Rml::Rectanglei region) override;

	static constexpr int MAX_ELEMENT_TRANSFORMS = 128;

	RID context_data_buffer;
	RID element_transform_buffer;


	struct ContextUniform {
		float projection[16];
	};

	struct ElementTransform {
		Projection transform;
	};

	ElementTransform element_transforms[MAX_ELEMENT_TRANSFORMS];
	int element_transform_count = 0;

	RD::DrawListID draw_list;

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

	using RenderCommand = std::variant<RenderGeometryCommand, SetTransformIndexCommand, ScissorEnableCommand, ScissorSetRegionCommand>;

	std::vector<RenderCommand> render_commands;
	static constexpr int SHARED_UNIFORM_SET_IDX = 0;
	static constexpr int TEXTURE_SHADER_UNIFORM_SET_IDX = 1;
	std::optional<BoundShader> currently_bound_shader;

	struct ScissorState {
		bool enabled = false;
		std::optional<Rect2i> region;
	} scissor_state;

	LocalVector<GeometryView *> geometry_deletion_queue;
	LocalVector<Texture *> texture_deletion_queue;

public:
	void flush_element_transforms_buffer();
	void execute_command(const RenderGeometryCommand &p_command);
	void execute_command(const SetTransformIndexCommand &p_command);
	void execute_command(const ScissorEnableCommand &p_command);
	void execute_command(const ScissorSetRegionCommand &p_command);
	
	void set_projection(const Projection &p_projection);
	void rebind_shared_uniforms(RID p_shader);
	RenderInterface_Godot_RD();
    virtual ~RenderInterface_Godot_RD();
};