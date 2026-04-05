#include "godot_rmlui_shaders.h"
#include "core/error/error_macros.h"
#include "core/templates/hashfuncs.h"
#include "rmlui/backend/shaders/color.glsl.gen.h"
#include "servers/rendering/renderer_rd/pipeline_cache_rd.h"

const char *GodotRmlUiShaders::render_shader_variant_2_str[] = {
    "normal",
    "stencil compare",
    "stencil replace",
    "stencil increase"
};

const char *GodotRmlUiShaders::render_shader_2_str[] = {
    "color",
    "texture"
};

void GodotRmlUiShaders::initialize(RD::FramebufferFormatID p_fb_format, RD::VertexFormatID p_vertex_format) {
    Vector<String> variants;
    variants.push_back("");

    auto init_compute_shader = [variants](auto *p_effect, ComputeShaderData &r_out) {
        p_effect->initialize(variants);
        RID shader_version = p_effect->version_create();
        RID shader_rid = p_effect->version_get_shader(shader_version, 0);

        r_out.shader = shader_rid;
        r_out.version = shader_version;
        r_out.pipeline = RD::get_singleton()->compute_pipeline_create(shader_rid);
        r_out.shader_rd = p_effect;
    };
    for (int i = 0; i < AvailableComputeShaderTypes::COMPUTE_SHADER_MAX; i++) {
        switch ((AvailableComputeShaderTypes)i) {
			case COMPUTE_SHADER_BLIT: {
                init_compute_shader(memnew(RmluiBlitShaderRD), compute_shaders[i]);
            } break;
			case COMPUTE_SHADER_MAX:
                DEV_ASSERT(false);
				break;
		}
    }

    auto init_render_shader = [variants, p_fb_format, p_vertex_format](auto *p_shader, const AvailableRenderShaderTypes p_shader_type, RenderShaderData &r_out) {
        p_shader->initialize(variants);
        RID shader_version = p_shader->version_create();
        RID shader_rid = p_shader->version_get_shader(shader_version, 0);

        r_out.shader = shader_rid;
        r_out.version = shader_version;
        r_out.shader_rd = p_shader;
        for (int i = 0; i < SHADER_VARIANT_MAX; i++) {
            RD::PipelineMultisampleState multisample_state = RD::PipelineMultisampleState();
            
            RD::PipelineDepthStencilState depth_stencil;
            RD::StencilOperation pass_stencil_op = RD::StencilOperation::STENCIL_OP_KEEP;
            RD::CompareOperator stencil_compare_op = RD::COMPARE_OP_ALWAYS;
            bool enable_stencil_writes = i > RenderShaderVariant::SHADER_VARIANT_STENCIL_COMPARE;
            bool enable_stencil_test = i == RenderShaderVariant::SHADER_VARIANT_STENCIL_COMPARE;
            int dynamic_state_flags = 0;
            RD::PipelineColorBlendState blend_state = enable_stencil_writes ? RD::PipelineColorBlendState::create_disabled() : RD::PipelineColorBlendState::create_blend(1);

            bool color_write_enabled = true;

            if (i == RenderShaderVariant::SHADER_VARIANT_STENCIL_COMPARE) {
                stencil_compare_op = RD::COMPARE_OP_EQUAL;
                dynamic_state_flags = RD::DYNAMIC_STATE_STENCIL_REFERENCE;
            } else if (i == RenderShaderVariant::SHADER_VARIANT_STENCIL_INCREASE) {
                pass_stencil_op = RD::STENCIL_OP_INCREMENT_AND_CLAMP;
                depth_stencil.back_op.reference = 1;
                depth_stencil.front_op.reference = 1;
                color_write_enabled = false;
            } else if (i == RenderShaderVariant::SHADER_VARIANT_STENCIL_REPLACE) {
                pass_stencil_op = RD::STENCIL_OP_REPLACE;
                dynamic_state_flags = RD::DYNAMIC_STATE_STENCIL_REFERENCE;
                color_write_enabled = false;
            }

            if (!color_write_enabled) {
                for (RD::PipelineColorBlendState::Attachment &attachment : blend_state.attachments) {
                    attachment.write_a = false;
                    attachment.write_r = false;
                    attachment.write_g = false;
                    attachment.write_b = false;
                }
            }

            depth_stencil.enable_stencil = enable_stencil_writes || enable_stencil_test;
            depth_stencil.front_op.fail = RD::STENCIL_OP_KEEP;
            depth_stencil.front_op.pass = pass_stencil_op;
            depth_stencil.front_op.depth_fail = RD::STENCIL_OP_KEEP;
            depth_stencil.front_op.compare = stencil_compare_op;
            depth_stencil.front_op.compare_mask = enable_stencil_test ? 0xFF : 0;
            depth_stencil.front_op.write_mask = enable_stencil_writes ? 0xFF : 0;

            depth_stencil.back_op = depth_stencil.front_op;

            r_out.pipelines[i].setup(r_out.shader, RD::RENDER_PRIMITIVE_TRIANGLES, RD::PipelineRasterizationState(), RD::PipelineMultisampleState(), depth_stencil, blend_state, dynamic_state_flags);
            r_out.precompiled_pipelines[i] = r_out.pipelines[i].get_render_pipeline(p_vertex_format, p_fb_format);
            RD::get_singleton()->set_resource_name(r_out.precompiled_pipelines[i], vformat("%s, %s", render_shader_2_str[p_shader_type], render_shader_variant_2_str[i]));
        }
    };

    for (int i = 0; i < AvailableRenderShaderTypes::RENDER_SHADER_MAX; i++) {
        AvailableRenderShaderTypes shader_type = (AvailableRenderShaderTypes)i;
        switch (shader_type) {
			case RENDER_SHADER_COLOR: {
                init_render_shader(memnew(ColorShaderRD), shader_type, render_shaders[i]);
            } break;
			case RENDER_SHADER_TEXTURE: {
                init_render_shader(memnew(TextureShaderRD), shader_type, render_shaders[i]);
            } break;
			case RENDER_SHADER_MAX:
                DEV_ASSERT(false);
				break;
		}
	}
}

RID GodotRmlUiShaders::get_compute_pipeline(const AvailableComputeShaderTypes p_shader) {
	ERR_FAIL_INDEX_V(p_shader, compute_shaders.size(), RID());
    return compute_shaders[p_shader].pipeline;
}

RID GodotRmlUiShaders::get_compute_shader(const AvailableComputeShaderTypes p_shader) {
	ERR_FAIL_INDEX_V(p_shader, compute_shaders.size(), RID());
	return compute_shaders[p_shader].shader;
}

RID GodotRmlUiShaders::get_render_pipeline(const AvailableRenderShaderTypes p_shader, const RenderShaderVariant p_variant, const RD::FramebufferFormatID p_fb_format, const RD::VertexFormatID p_vertex_format) {
	ERR_FAIL_INDEX_V(p_shader, render_shaders.size(), RID());
	ERR_FAIL_INDEX_V(p_variant, SHADER_VARIANT_MAX, RID());
    RID pippaline = render_shaders[p_shader].precompiled_pipelines[p_variant];

    return pippaline;
}

RID GodotRmlUiShaders::get_render_shader(const AvailableRenderShaderTypes p_shader) {
	ERR_FAIL_INDEX_V(p_shader, render_shaders.size(), RID());
	return render_shaders[p_shader].shader;
}

GodotRmlUiShaders::RmluiPipelineID GodotRmlUiShaders::get_pipeline_id(const AvailableRenderShaderTypes p_shader, const RenderShaderVariant p_variant, const RD::FramebufferFormatID p_fb_format, const RD::VertexFormatID p_vertex_format) const {
    uint32_t h = hash_murmur3_one_32(p_shader);
    h = hash_murmur3_one_32(p_variant, h);
    h = hash_murmur3_one_64(p_fb_format, h);
    h = hash_murmur3_one_64(p_vertex_format, h);
    return hash_fmix32(h);
}

GodotRmlUiShaders::~GodotRmlUiShaders() {
    RD *rd = RD::get_singleton();
    for (int i = 0; i < AvailableComputeShaderTypes::COMPUTE_SHADER_MAX; i++) {
        rd->free_rid(compute_shaders[i].pipeline);
        compute_shaders[i].shader_rd->version_free(compute_shaders[i].version);
        memdelete(compute_shaders[i].shader_rd);
    }

    for (int i = 0; i < AvailableRenderShaderTypes::RENDER_SHADER_MAX; i++) {
        for (int j = 0; j < SHADER_VARIANT_MAX; j++) {
            render_shaders[i].pipelines[j].clear();
        }
        render_shaders[i].shader_rd->version_free(render_shaders[i].version);
        memdelete(render_shaders[i].shader_rd);
    }
}
