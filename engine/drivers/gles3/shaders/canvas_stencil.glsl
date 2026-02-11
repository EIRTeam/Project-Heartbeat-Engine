/* clang-format off */
#[modes]

mode_default =

#[specializations]

#[vertex]

layout(location = 0) in vec2 vertex;

uniform highp vec4 rect;
uniform highp mat4 projection_matrix;
uniform highp mat4 view_matrix;
uniform highp mat4 canvas_transform_for_3d;
uniform highp mat4 canvas_transform;
uniform highp mat4 screen_transform_3d;

void main() {
	// crash on Adreno 320/330
	//vec2 vertex_base_arr[6] = vec2[](vec2(0.0, 0.0), vec2(0.0, 1.0), vec2(1.0, 1.0), vec2(1.0, 0.0), vec2(0.0, 0.0), vec2(1.0, 1.0));
	//vec2 vertex_base = vertex_base_arr[gl_VertexID % 6];
	//-----------------------------------------
	// ID |  0  |  1  |  2  |  3  |  4  |  5  |
	//-----------------------------------------
	// X  | 0.0 | 0.0 | 1.0 | 1.0 | 0.0 | 1.0 |
	// Y  | 0.0 | 1.0 | 1.0 | 0.0 | 0.0 | 1.0 |
	//-----------------------------------------
	// no crash or freeze on all Adreno 3xx	with 'if / else if' and slightly faster!
	int vertex_id = gl_VertexID % 6;
	vec2 vertex_base;
	if (vertex_id == 0)
		vertex_base = vec2(0.0, 0.0);
	else if (vertex_id == 1)
		vertex_base = vec2(0.0, 1.0);
	else if (vertex_id == 2)
		vertex_base = vec2(1.0, 1.0);
	else if (vertex_id == 3)
		vertex_base = vec2(1.0, 0.0);
	else if (vertex_id == 4)
		vertex_base = vec2(0.0, 0.0);
	else if (vertex_id == 5)
		vertex_base = vec2(1.0, 1.0);
	vec2 vertex_2d = vertex_base;

	vertex_2d.xy *= rect.zw;
	vertex_2d.xy += rect.xy;
	gl_Position = projection_matrix * view_matrix * canvas_transform_for_3d * screen_transform_3d * vec4(vertex_2d, 0.0, 1.0);
}

#[fragment]

out vec4 color;

void main() {
	color = vec4(1.0, 0.0, 0.0, 1.0);
}
