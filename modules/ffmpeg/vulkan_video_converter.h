#ifndef FFMPEG_VULKAN_VIDEO_CONVERTER_H
#define FFMPEG_VULKAN_VIDEO_CONVERTER_H

#if defined(FFMPEG_GPU_DECODE_ENABLED) && !defined(GDEXTENSION)
#include "core/object/ref_counted.h"
#include "core/templates/safe_refcount.h"
#include "ffmpeg_frame.h"
#include "scene/resources/texture_rd.h"

// Engine-only GPU conversion. The render graph owns the output image's
// transitions; per-frame jobs retain the FFmpeg image until the RD fence.
class VulkanVideoConverter : public RefCounted {
	Ref<Texture2DRD> output;
	Ref<RefCounted> state;
	SafeFlag failed;
	static void _convert_render_thread(Ref<RefCounted> p_self, Ref<FFmpegFrame> p_frame);
	static void _free_render_thread(Ref<RefCounted> p_state, Ref<Texture2DRD> p_output);
	static void _clear_render_thread(Ref<RefCounted> p_self);

public:
#ifdef FFMPEG_GPU_TESTS_ENABLED
	void test_force_failure() { failed.set(); }
	static Dictionary get_statistics(RID p_texture = RID());
	static void enable_color_validation();
#endif
	VulkanVideoConverter();
	~VulkanVideoConverter();
	void convert(const Ref<FFmpegFrame> &p_frame);
	void clear();
	Ref<Texture2DRD> get_texture() const { return output; }
	bool has_failed() const { return failed.is_set(); }
};
#endif
#endif
