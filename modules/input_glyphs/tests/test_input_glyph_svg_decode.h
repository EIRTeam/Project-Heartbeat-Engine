#pragma once

#include "../input_glyph_svg_decode.h"
#include "tests/test_macros.h"

namespace TestInputGlyphSVGDecode {

TEST_CASE("[InputGlyphs] SVG rasterization preserves requested size and RGBA colors") {
	const PackedByteArray svg = String("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"2\" height=\"1\"><rect width=\"1\" height=\"1\" fill=\"#ff0000\"/><rect x=\"1\" width=\"1\" height=\"1\" fill=\"#0000ff\"/></svg>").to_utf8_buffer();
	Ref<Image> image;
	image.instantiate();
	REQUIRE(InputGlyphSVGDecode::render_svg(image, svg, Vector2i(20, 10)) == OK);
	CHECK(image->get_size() == Vector2i(20, 10));
	CHECK(image->get_format() == Image::FORMAT_RGBA8);
	CHECK(image->get_pixel(5, 5).is_equal_approx(Color(1, 0, 0, 1)));
	CHECK(image->get_pixel(15, 5).is_equal_approx(Color(0, 0, 1, 1)));
}

} // namespace TestInputGlyphSVGDecode
