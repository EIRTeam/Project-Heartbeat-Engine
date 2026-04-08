#pragma once

#include "rmlui/backend/shaders/color.glsl.gen.h"
#include "rmlui/backend/shaders/texture.glsl.gen.h"
#include "servers/rendering/renderer_rd/pipeline_cache_rd.h"
#include "shaders/rmlui_blit.glsl.gen.h"
#include "shaders/passthrough.glsl.gen.h"
#include "shaders/rmlui_blur.glsl.gen.h"
#include <variant>
#include <array>
#include "shaders/rmlui_blur_settings_inc.glsl"

class GodotRmlUiShaders {

    using AvailableComputeShaders = std::variant<
        RmluiBlitShaderRD
    >;

    using AvailableRenderShaders = std::variant<
        ColorShaderRD,
		TextureShaderRD,
		PassthroughShaderRD,
		RmluiBlurShaderRD
    >;
public:
	enum AvailableComputeShaderTypes {
		COMPUTE_SHADER_BLIT,
		COMPUTE_SHADER_BLIT_TEXTURE,
		COMPUTE_SHADER_MAX
	};
	enum AvailableRenderShaderTypes {
		RENDER_SHADER_COLOR,
		RENDER_SHADER_TEXTURE,
		RENDER_SHADER_PASSTHROUGH,
		RENDER_SHADER_BLUR,
		RENDER_SHADER_MAX
	};

	static const char* render_shader_2_str[RENDER_SHADER_MAX];

	enum RenderShaderVariant {
		SHADER_VARIANT_NORMAL,
		SHADER_VARIANT_NORMAL_NO_BLEND,
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
	struct BlurPushConstant {
		Vector2 texel_offset;
		float weights[BLUR_NUM_WEIGHTS];
		Vector2 tex_coord_min;
		Vector2 tex_coord_max;
		uint8_t padding[8];
	};

	using RmluiPipelineID = uint64_t;
    void initialize(RD::FramebufferFormatID p_fb_format, RD::VertexFormatID p_vertex_format);
	RID get_compute_pipeline(const AvailableComputeShaderTypes p_shader);
	RID get_compute_shader(const AvailableComputeShaderTypes p_shader);
	RID get_render_pipeline(const AvailableRenderShaderTypes p_shader, const RenderShaderVariant p_variant, const RD::FramebufferFormatID p_fb_format, const RD::VertexFormatID p_vertex_format);
	RID get_render_shader(const AvailableRenderShaderTypes p_shader);
	RmluiPipelineID get_pipeline_id(const AvailableRenderShaderTypes p_shader, const RenderShaderVariant p_variant, const RD::FramebufferFormatID p_fb_format, const RD::VertexFormatID p_vertex_format) const;
    static void set_blur_weights(BlurPushConstant &r_blur, float p_sigma);
	static void set_blur_tex_coord_limits(BlurPushConstant &r_blur, const Rect2i &p_rect, const Vector2 &p_framebuffer_size);
	~GodotRmlUiShaders();
};