#version 440

#include "rmlui_blur_settings_inc.glsl"

layout(push_constant, std430) uniform Pos {
	vec2 texel_offset;
	float weights[BLUR_NUM_WEIGHTS];
    vec2 tex_coord_min;
    vec2 tex_coord_max;
}
data;

layout (location = 0) in vec2 inPosition;
layout (location = 1) in vec4 inColor0;
layout (location = 2) in vec2 inTexCoord0;

layout (location = 0) out vec2 fragTexCoord[BLUR_SIZE];

void main() {
	for(int i = 0; i < BLUR_SIZE; i++) {
		fragTexCoord[i] = inTexCoord0 - float(i - BLUR_NUM_WEIGHTS + 1) * data.texel_offset;
    }
    gl_Position = vec4(vec3(inPosition, 0.0), 1.0);
}