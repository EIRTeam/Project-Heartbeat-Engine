#pragma once

#include "../input_glyphs_constants.h"
#include "tests/test_macros.h"

namespace TestInputGlyphsConstants {

TEST_CASE("[InputGlyphs] SDL button lookup preserves existing glyph origins") {
	CHECK(InputGlyphsConstants::godot_button_to_input_origin_lut[(int)JoyButton::A] == InputGlyphsConstants::INPUT_ORIGIN_A);
	CHECK(InputGlyphsConstants::godot_button_to_input_origin_lut[(int)JoyButton::PADDLE4] == InputGlyphsConstants::INPUT_ORIGIN_PADDLE4);
	CHECK(InputGlyphsConstants::godot_button_to_input_origin_lut[(int)JoyButton::TOUCHPAD] == InputGlyphsConstants::INPUT_ORIGIN_TRACKPAD_CLICK);
}

TEST_CASE("[InputGlyphs] Additional SDL miscellaneous buttons have no glyph origin") {
	for (int button = (int)JoyButton::MISC2; button <= (int)JoyButton::MISC6; button++) {
		CHECK(InputGlyphsConstants::godot_button_to_input_origin_lut[button] == InputGlyphsConstants::INPUT_ORIGIN_INVALID);
	}
}

} // namespace TestInputGlyphsConstants
