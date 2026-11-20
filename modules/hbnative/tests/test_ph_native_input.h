#pragma once

#include "../ph_singleton.h"
#include "core/os/os.h"
#include "tests/test_macros.h"

namespace TestPHNativeInput {

TEST_CASE("[PHNative][SceneTree] Input clock advances monotonically in microseconds") {
	const uint64_t before = PHNative::get_clock_time_usec();
	OS::get_singleton()->delay_usec(2000);
	const uint64_t after = PHNative::get_clock_time_usec();
	CHECK(after > before);
}

TEST_CASE("[PHNative][SceneTree] Disconnected SDL device queries have safe defaults") {
	CHECK_FALSE(PHNative::is_sdl_device_game_controller(-1));
	CHECK(PHNative::get_sdl_device_guid(-1).is_empty());
}

} // namespace TestPHNativeInput
