/**************************************************************************/
/*  input_glyph_svg_decode.cpp                                            */
/**************************************************************************/
/*                         This file is part of:                          */
/*                          EIRTeam.InputGlyphs                           */
/*                         https://ph.eirteam.moe                         */
/**************************************************************************/
/* Copyright (c) 2023-present Álex Román (EIRTeam) & contributors.        */
/*                                                                        */
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

#include "input_glyph_svg_decode.h"
#include <thorvg.h>

Error InputGlyphSVGDecode::render_svg(Ref<Image> p_image, const PackedByteArray &p_buffer, const Vector2i &p_dimensions) {
	ERR_FAIL_COND_V_MSG(p_dimensions == Vector2i(), ERR_INVALID_PARAMETER, "Steamworks Glyphs SVG: Can't load SVG with a scale of 0.");

	tvg::Picture *picture = tvg::Picture::gen();

	tvg::Result result = picture->load((const char *)p_buffer.ptr(), p_buffer.size(), "svg", nullptr, true);
	if (result != tvg::Result::Success) {
		tvg::Paint::rel(picture);
		return ERR_INVALID_DATA;
	}
	uint32_t width = MAX(1, p_dimensions.x);
	uint32_t height = MAX(1, p_dimensions.y);

	const uint32_t max_dimension = 16384;
	if (width > max_dimension || height > max_dimension) {
		WARN_PRINT(vformat(
				String::utf8("Steamworks SVG: Target canvas dimensions %d×%d exceed the max supported dimensions %d×%d. The target canvas will be scaled down."),
				width, height, max_dimension, max_dimension));
		width = MIN(width, max_dimension);
		height = MIN(height, max_dimension);
	}

	picture->size(width, height);

	std::unique_ptr<tvg::SwCanvas> sw_canvas(tvg::SwCanvas::gen());
	Vector<uint8_t> buffer;
	buffer.resize(sizeof(uint32_t) * width * height);

	tvg::Result res = sw_canvas->target((uint32_t *)buffer.ptrw(), width, width, height, tvg::ColorSpace::ABGR8888S);
	if (res != tvg::Result::Success) {
		tvg::Paint::rel(picture);
		ERR_FAIL_V_MSG(FAILED, "ImageLoaderSVG: Couldn't set target on ThorVG canvas.");
	}

	res = sw_canvas->add(picture);
	if (res != tvg::Result::Success) {
		ERR_FAIL_V_MSG(FAILED, "ImageLoaderSVG: Couldn't insert ThorVG picture on canvas.");
	}

	res = sw_canvas->draw(true);
	if (res != tvg::Result::Success) {
		ERR_FAIL_V_MSG(FAILED, "ImageLoaderSVG: Couldn't draw ThorVG pictures on canvas.");
	}

	res = sw_canvas->sync();
	if (res != tvg::Result::Success) {
		ERR_FAIL_V_MSG(FAILED, "ImageLoaderSVG: Couldn't sync ThorVG canvas.");
	}

	p_image->set_data(width, height, false, Image::FORMAT_RGBA8, buffer);
	return OK;
}
