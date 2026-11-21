#[compute]
#version 450

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
layout(set = 0, binding = 0) uniform sampler2D y_plane;
layout(set = 0, binding = 1) uniform sampler2D uv_plane;
layout(rgba8, set = 0, binding = 2) uniform writeonly image2D output_image;
layout(push_constant, std430) uniform Params {
	vec4 range; // Y offset, UV center, Y scale, UV scale.
	vec4 coefficients; // Rv, Gu, Gv, Bu.
	ivec2 size;
} params;

void main() {
	ivec2 p = ivec2(gl_GlobalInvocationID.xy);
	if (any(greaterThanEqual(p, params.size))) {
		return;
	}
	float y = (texelFetch(y_plane, p, 0).r - params.range.x) * params.range.z;
	vec2 uv = (texelFetch(uv_plane, p / 2, 0).rg - params.range.y) * params.range.w;
	vec3 rgb = vec3(y + params.coefficients.x * uv.y,
			y + params.coefficients.y * uv.x + params.coefficients.z * uv.y,
			y + params.coefficients.w * uv.x);
	imageStore(output_image, p, vec4(clamp(rgb, 0.0, 1.0), 1.0));
}
