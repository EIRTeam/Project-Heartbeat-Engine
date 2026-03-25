/**************************************************************************/
/*  color.glsl.gen.h                                                      */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

/* THIS FILE IS GENERATED. EDITS WILL BE LOST. */

#pragma once

#include "servers/rendering/renderer_rd/shader_rd.h"

class ColorShaderRD : public ShaderRD {
public:
	ColorShaderRD() {
		static const char _vertex_code[] = {
R"<!>(
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

)<!>"
		};
		static const char _fragment_code[] = {
R"<!>(
#version 330

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) in vec2 fragTexCoord;
layout (location = 1) in vec4 fragColor;
layout (location = 0) out vec4 finalColor;

void main() {
	finalColor = fragColor;
}

)<!>"
		};
		static const char *_compute_code = nullptr;
		setup(_vertex_code, _fragment_code, _compute_code, "ColorShaderRD");
	}
};
