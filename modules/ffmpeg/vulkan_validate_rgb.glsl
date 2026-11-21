#[compute]
#version 450
layout(local_size_x = 8, local_size_y = 1, local_size_z = 1) in;
layout(rgba8, set = 0, binding = 2) uniform readonly image2D output_image;
layout(std430, set = 0, binding = 3) buffer Result {
	uint checked;
	uint failures;
} result;
layout(push_constant, std430) uniform Params {
	vec4 range;
	vec4 coefficients;
	ivec2 size;
} params;
// Independent RGB reference for the generated eight full-intensity bars.
const vec3 expected[8] = vec3[8](
	vec3(1, 1, 1), vec3(1, 1, 0), vec3(0, 1, 1), vec3(0, 1, 0),
	vec3(1, 0, 1), vec3(1, 0, 0), vec3(0, 0, 1), vec3(0, 0, 0));
void main() {
	uint bar = gl_GlobalInvocationID.x;
	ivec2 p = ivec2((int(bar) * 2 + 1) * params.size.x / 16, params.size.y / 2);
	vec4 pixel = imageLoad(output_image, p);
	if (any(greaterThan(abs(pixel.rgb - expected[bar]), vec3(5.0 / 255.0))) ||
			abs(pixel.a - 1.0) > 1.0 / 255.0) {
		atomicAdd(result.failures, 1);
	}
	atomicAdd(result.checked, 1);
}
