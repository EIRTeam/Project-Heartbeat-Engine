#[compute]

#version 450

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(push_constant, std430) uniform Pos {
	ivec2 src_pos;
	ivec2 dst_pos;
    ivec2 copy_size;
    uint flip_vertical;
}
data;

layout(binding = 0, rgba8) readonly uniform image2D src_rt;
layout(binding = 1, rgba8) writeonly uniform image2D dest_rt;

void main() {
    ivec2 image_source_size = imageSize(src_rt);
    ivec2 image_destination_size = imageSize(dest_rt);
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy);

    ivec2 destination_pixel = data.dst_pos + uv;

    if (any(greaterThanEqual(destination_pixel, image_destination_size))) {
        return;
    }

    if (data.flip_vertical != 0) {
        destination_pixel = data.dst_pos + ivec2(0, data.copy_size.y - 1) + ivec2(uv.x, -uv.y);
    }

    ivec2 source_pixel = data.src_pos + uv;

    vec4 color = imageLoad(src_rt, source_pixel);
    imageStore(dest_rt, destination_pixel, color);
}
