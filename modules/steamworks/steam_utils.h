/**************************************************************************/
/*  steam_utils.h                                                         */
/**************************************************************************/
/*                         This file is part of:                          */
/*                           EIRTeam.Steamworks                           */
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

#ifndef STEAM_UTILS_H
#define STEAM_UTILS_H

#include "core/object/ref_counted.h"
#include "steamworks_callback_data.h"
#include "steamworks_constants.gen.h"

class ISteamUtils;

class HBSteamUtils : public RefCounted {
	GDCLASS(HBSteamUtils, RefCounted);

private:
	ISteamUtils *steam_utils = nullptr;
	void _on_gamepad_text_input_dismissed(Ref<SteamworksCallbackData> p_callback);
	void _on_floating_gamepad_text_input_dismissed(Ref<SteamworksCallbackData> p_callback);

protected:
	static void _bind_methods();

public:
	bool is_in_big_picture_mode() const;
	bool is_on_steam_deck() const;
	bool show_gamepad_text_input(SWC::GamepadTextInputMode p_input_mode, SWC::GamepadTextInputLineMode p_line_input_mode, String p_description, String p_existing_text, uint32_t p_max_text) const;
	bool show_floating_gamepad_text_input(SWC::FloatingGamepadTextInputMode p_input_mode, Rect2i p_text_field_rect) const;
	bool is_overlay_enabled() const;
	void init_interface();
	ISteamUtils *get_interface();
	bool is_valid() const;
	HBSteamUtils();
};

#endif // STEAM_UTILS_H
