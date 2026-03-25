#version 440

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(push_constant, std430) uniform Params {
	vec2 translate;
	int transform_idx;
}
userdata;

layout(set = 0, binding = 0, std430) buffer ContextParams {
	mat4 projection;
} context_params;

layout(set = 0, binding = 1, std430) buffer ElementTransform {
	mat4 transform;
} element_transforms[];

layout (location = 0) in vec2 inPosition;
layout (location = 1) in vec4 inColor0;
layout (location = 2) in vec2 inTexCoord0;

layout (location = 0) out vec2 fragTexCoord;
layout (location = 1) out vec4 fragColor;

const mat4 identity = mat4(1.0, 0.0, 0.0, 0.0,
						   0.0, 1.0, 0.0, 0.0,
						   0.0, 0.0, 1.0, 0.0,
						   0.0, 0.0, 0.0, 1.0);

void main() {
	fragTexCoord = inTexCoord0;
	fragColor = inColor0;
	vec2 translatedPos = inPosition + userdata.translate.xy;
	mat4 element_trf = (userdata.transform_idx >= 0 ? element_transforms[0].transform : identity);
	vec4 outPos = context_params.projection * element_trf * vec4(translatedPos, 0, 1);
    gl_Position = outPos;
}