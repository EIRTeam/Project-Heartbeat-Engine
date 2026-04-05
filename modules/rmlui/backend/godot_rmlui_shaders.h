#pragma once

#include "rmlui/backend/shaders/color.glsl.gen.h"
#include "rmlui/backend/shaders/texture.glsl.gen.h"
#include "servers/rendering/renderer_rd/pipeline_cache_rd.h"
#include "shaders/rmlui_blit.glsl.gen.h"
#include <variant>
#include <array>

class GodotRmlUiShaders {

    using AvailableComputeShaders = std::variant<
        RmluiBlitShaderRD
    >;

    using AvailableRenderShaders = std::variant<
        ColorShaderRD,
		TextureShaderRD
    >;
public:
	enum AvailableComputeShaderTypes {
		COMPUTE_SHADER_BLIT,
		COMPUTE_SHADER_MAX
	};
	enum AvailableRenderShaderTypes {
		RENDER_SHADER_COLOR,
		RENDER_SHADER_TEXTURE,
		RENDER_SHADER_MAX
	};

	static const char* render_shader_2_str[RENDER_SHADER_MAX];

	enum RenderShaderVariant {
		SHADER_VARIANT_NORMAL,
		SHADER_VARIANT_STENCIL_COMPARE,
		SHADER_VARIANT_STENCIL_REPLACE,
		SHADER_VARIANT_STENCIL_INCREASE,
		SHADER_VARIANT_MAX
	};

	static const char* render_shader_variant_2_str[SHADER_VARIANT_MAX];
private:
	struct ComputeShaderData {
		RID version;
		RID shader;
		RID pipeline;
        ShaderRD *shader_rd;
	};

	std::array<ComputeShaderData, COMPUTE_SHADER_MAX> compute_shaders;
	
	struct RenderShaderData {
		RID version;
		RID shader;
		std::array<PipelineCacheRD, SHADER_VARIANT_MAX> pipelines;
		std::array<RID, SHADER_VARIANT_MAX> precompiled_pipelines;
        ShaderRD *shader_rd;
	};
	
	std::array<RenderShaderData, RENDER_SHADER_MAX> render_shaders;
public:
	using RmluiPipelineID = uint64_t;
    void initialize(RD::FramebufferFormatID p_fb_format, RD::VertexFormatID p_vertex_format);
	RID get_compute_pipeline(const AvailableComputeShaderTypes p_shader);
	RID get_compute_shader(const AvailableComputeShaderTypes p_shader);
	RID get_render_pipeline(const AvailableRenderShaderTypes p_shader, const RenderShaderVariant p_variant, const RD::FramebufferFormatID p_fb_format, const RD::VertexFormatID p_vertex_format);
	RID get_render_shader(const AvailableRenderShaderTypes p_shader);
	RmluiPipelineID get_pipeline_id(const AvailableRenderShaderTypes p_shader, const RenderShaderVariant p_variant, const RD::FramebufferFormatID p_fb_format, const RD::VertexFormatID p_vertex_format) const;
    ~GodotRmlUiShaders();
};