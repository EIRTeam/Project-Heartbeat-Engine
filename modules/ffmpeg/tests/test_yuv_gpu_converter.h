#pragma once

#include "../ffmpeg_video_stream.h"

#include "core/os/thread.h"
#include "servers/rendering/rendering_server.h"
#include "tests/test_macros.h"

namespace TestYUVGPUConverter {

// The dummy renderer is sufficient to exercise queued ownership without a GPU.
// Joining the worker leaves its commands pending on the main/render thread.
static void queue_clear(void *p_userdata) {
	Ref<YUVGPUConverter> &converter = *static_cast<Ref<YUVGPUConverter> *>(p_userdata);
	converter->clear_output_texture();
	converter->clear_output_texture();
	converter.unref();
}

TEST_CASE("[FFmpeg][SceneTree] Queued clears retain the converter until execution") {
	Ref<YUVGPUConverter> converter;
	converter.instantiate();
	ObjectID id = converter->get_instance_id();
	Ref<Texture2D> output = converter->get_output_texture();
	REQUIRE(output.is_valid());

	Thread worker;
	worker.start(queue_clear, &converter);
	worker.wait_to_finish();

	CHECK(converter.is_null());
	CHECK(ObjectDB::get_instance(id) != nullptr);
	RenderingServer::get_singleton()->sync();
	CHECK(ObjectDB::get_instance(id) == nullptr);
	// The public wrapper may outlive the converter.
	CHECK(output.is_valid());
}

TEST_CASE("[FFmpeg][SceneTree] Unused converter can be released off the render thread") {
	Ref<YUVGPUConverter> converter;
	converter.instantiate();
	ObjectID id = converter->get_instance_id();
	Thread worker;
	worker.start([](void *p_userdata) {
		static_cast<Ref<YUVGPUConverter> *>(p_userdata)->unref();
	},
			&converter);
	worker.wait_to_finish();

	CHECK(ObjectDB::get_instance(id) == nullptr);
	RenderingServer::get_singleton()->sync();
}

} // namespace TestYUVGPUConverter
