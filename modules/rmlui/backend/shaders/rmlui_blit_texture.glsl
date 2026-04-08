#[compute]

#version 450

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(push_constant, std430) uniform Pos {
	ivec2 src_pos;
	ivec2 src_size;
	ivec2 dst_pos;
    ivec2 target_size;
    uint flip_vertical;
}
data;

layout(set = 0, binding = 0) uniform sampler2D src_rt;
layout(set = 0, binding = 1, rgba8) writeonly uniform image2D dest_rt;

void main() {
    ivec2 image_source_size = textureSize(src_rt, 0);
    ivec2 image_destination_size = imageSize(dest_rt);
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy);

    ivec2 destination_pixel = data.dst_pos + uv;

    if (any(greaterThanEqual(destination_pixel, image_destination_size)) || any(greaterThanEqual(destination_pixel, data.dst_pos + data.target_size))) {
        return;
    }

    if (data.flip_vertical != 0) {
        destination_pixel = data.dst_pos + ivec2(0, data.target_size.y - 1) + ivec2(uv.x, -uv.y);
    }

    vec2 source_size = vec2(image_source_size);
    // Shift origin to texel center and clamp to prevent bleed
    vec2 src_uv_min = (vec2(data.src_pos) + 0.5) / source_size;
    vec2 src_uv_max = (vec2(data.src_pos + data.src_size) - 0.5) / source_size;
    vec2 src_uv = src_uv_min + (vec2(uv) / vec2(data.target_size)) * (src_uv_max - src_uv_min);
    src_uv = clamp(src_uv, src_uv_min, src_uv_max);
    vec4 color = texture(src_rt, src_uv);
    imageStore(dest_rt, destination_pixel, color);
}
