#version 440

#include "rmlui_blur_settings_inc.glsl"

layout(set = 0, binding = 0) uniform sampler2D _tex;

layout(push_constant, std430) uniform Pos {
	vec2 texel_offset;
	float weights[BLUR_NUM_WEIGHTS];
    vec2 tex_coord_min;
    vec2 tex_coord_max;
}
data;

layout(location = 0) in vec2 fragTexCoord[BLUR_SIZE];
layout(location = 0) out vec4 finalColor;

void main() {
	vec4 color = vec4(0.0, 0.0, 0.0, 0.0);
	for(int i = 0; i < BLUR_SIZE; i++)
	{
		vec2 in_region = step(data.tex_coord_min, fragTexCoord[i]) * step(fragTexCoord[i], data.tex_coord_max);
		color += texture(_tex, fragTexCoord[i]) * in_region.x * in_region.y * data.weights[abs(i - BLUR_NUM_WEIGHTS + 1)];
	}
	finalColor = color;
}