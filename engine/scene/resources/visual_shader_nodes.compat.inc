/**************************************************************************/
/*  visual_shader_nodes.compat.inc                                        */
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

#ifndef DISABLE_DEPRECATED

// VisualShaderNodeCubemap

void VisualShaderNodeCubemap::_set_cube_map_bind_compat_95126(Ref<Cubemap> p_cube_map) {
	set_cube_map(p_cube_map);
}

Ref<Cubemap> VisualShaderNodeCubemap::_get_cube_map_bind_compat_95126() const {
	return cube_map;
}

void VisualShaderNodeCubemap::_bind_compatibility_methods() {
	ClassDB::bind_compatibility_method(D_METHOD("set_cube_map", "value"), &VisualShaderNodeCubemap::_set_cube_map_bind_compat_95126);
	ClassDB::bind_compatibility_method(D_METHOD("get_cube_map"), &VisualShaderNodeCubemap::_get_cube_map_bind_compat_95126);
}

// VisualShaderNodeTexture2DArray

void VisualShaderNodeTexture2DArray::_set_texture_array_bind_compat_95126(Ref<Texture2DArray> p_texture_array) {
	set_texture_array(p_texture_array);
}

Ref<Texture2DArray> VisualShaderNodeTexture2DArray::_get_texture_array_bind_compat_95126() const {
	return texture_array;
}

void VisualShaderNodeTexture2DArray::_bind_compatibility_methods() {
	ClassDB::bind_compatibility_method(D_METHOD("set_texture_array", "value"), &VisualShaderNodeTexture2DArray::_set_texture_array_bind_compat_95126);
	ClassDB::bind_compatibility_method(D_METHOD("get_texture_array"), &VisualShaderNodeTexture2DArray::_get_texture_array_bind_compat_95126);
}

#endif // DISABLE_DEPRECATED