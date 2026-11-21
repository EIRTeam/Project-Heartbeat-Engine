/**************************************************************************/
/*  video_decoder.cpp                                                     */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             EIRTeam.FFmpeg                             */
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

#if defined(FFMPEG_GPU_DECODE_ENABLED) && !defined(GDEXTENSION)
#include "drivers/vulkan/godot_vulkan.h"
#endif

#include "video_decoder.h"
#include "ffmpeg_frame.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavcodec/codec.h"
#include "libavcodec/codec_id.h"
#include "libavformat/avformat.h"
#ifdef FFMPEG_GPU_DECODE_ENABLED
#include "libavutil/hwcontext.h"
#include "libavutil/hwcontext_vulkan.h"
#endif
#include "libswresample/swresample.h"
#include "libswscale/swscale.h"
}
#include "tracy_import.h"
#include <cstdio>
#include <cstdint>
#include <iterator>
#include <limits>

#ifdef GDEXTENSION
#include "gdextension_build/gdex_print.h"
#include <godot_cpp/classes/rendering_server.hpp>
#else
#include "core/config/project_settings.h"
#include "servers/rendering/rendering_server.h"
#include "core/object/callable_mp.h"
#include "core/os/os.h"
#endif

#if defined(FFMPEG_GPU_DECODE_ENABLED) && !defined(GDEXTENSION)
#include "servers/rendering/rendering_device.h"
#include "vulkan_video_converter.h"
#endif

extern "C" {
#include "libavformat/avformat.h"
#include "libavformat/avio.h"
}

const int MAX_PENDING_FRAMES = 3;

#if defined(FFMPEG_GPU_DECODE_ENABLED) && !defined(GDEXTENSION)
static void _ffmpeg_vulkan_queue_lock(AVHWDeviceContext *p_device_context, uint32_t p_queue_family, uint32_t p_queue_index) {
	RenderingDevice *rd = static_cast<RenderingDevice *>(p_device_context->user_opaque);
	if (rd != nullptr) {
		rd->vulkan_video_decode_queue_lock(p_queue_family, p_queue_index);
	}
}

static void _ffmpeg_vulkan_queue_unlock(AVHWDeviceContext *p_device_context, uint32_t p_queue_family, uint32_t p_queue_index) {
	RenderingDevice *rd = static_cast<RenderingDevice *>(p_device_context->user_opaque);
	if (rd != nullptr) {
		rd->vulkan_video_decode_queue_unlock(p_queue_family, p_queue_index);
	}
}

bool VideoDecoder::_initialize_vulkan_device_context(int p_codec_id) {
#if !FF_API_VULKAN_SYNC_QUEUES
	print_verbose("FFmpeg Vulkan bridge requires caller-provided queue locking; this FFmpeg API cannot safely share the reserved queues.");
	return false;
#endif
	if (p_codec_id != AV_CODEC_ID_H264 && p_codec_id != AV_CODEC_ID_HEVC) {
		print_verbose(vformat("FFmpeg Vulkan bridge skipped unsupported codec ID %d.", p_codec_id));
		return false;
	}

	RenderingServer *rendering_server = RenderingServer::get_singleton();
	if (rendering_server == nullptr) {
		print_verbose("FFmpeg Vulkan bridge unavailable: RenderingServer is not initialized.");
		return false;
	}
	RenderingDevice *rd = rendering_server->get_rendering_device();
	if (rd == nullptr) {
		print_verbose("FFmpeg Vulkan bridge unavailable: no Vulkan RenderingDevice is active.");
		return false;
	}

	RenderingDevice::VulkanVideoDecodeDeviceInfo device_info;
	if (!rd->get_vulkan_video_decode_device_info(device_info)) {
		print_verbose("FFmpeg Vulkan bridge unavailable: the RenderingDevice has no dedicated Vulkan decode queue with required synchronization support.");
		return false;
	}

	const VkVideoCodecOperationFlagBitsKHR codec_operation = p_codec_id == AV_CODEC_ID_H264 ?
			VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR :
			VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR;
	if ((device_info.codec_operations & codec_operation) == 0) {
		print_verbose(vformat("FFmpeg Vulkan bridge unavailable: codec %s is not supported by the reserved decode queue.",
				avcodec_get_name(static_cast<AVCodecID>(p_codec_id))));
		return false;
	}

	av_buffer_unref(&vulkan_device_context);
	vulkan_device_context = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_VULKAN);
	if (vulkan_device_context == nullptr) {
		print_verbose("FFmpeg Vulkan bridge unavailable: av_hwdevice_ctx_alloc() failed.");
		return false;
	}

	AVHWDeviceContext *device_context = reinterpret_cast<AVHWDeviceContext *>(vulkan_device_context->data);
	AVVulkanDeviceContext *vulkan_context = static_cast<AVVulkanDeviceContext *>(device_context->hwctx);
	// FFmpeg's external-device init requires a loader callback; it only loads
	// libvulkan itself when it is also creating the Vulkan device.
	vulkan_context->get_proc_addr = vkGetInstanceProcAddr;
	vulkan_context->inst = reinterpret_cast<VkInstance>(device_info.instance);
	vulkan_context->phys_dev = reinterpret_cast<VkPhysicalDevice>(device_info.physical_device);
	vulkan_context->act_dev = reinterpret_cast<VkDevice>(device_info.logical_device);
	vulkan_context->device_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	static VkPhysicalDeviceVulkan13Features enabled_vulkan_1_3_features = [] {
		VkPhysicalDeviceVulkan13Features features = {};
		features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
		features.synchronization2 = VK_TRUE;
		return features;
	}();
	static VkPhysicalDeviceVulkan12Features enabled_vulkan_1_2_features = [] {
		VkPhysicalDeviceVulkan12Features features = {};
		features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
		features.pNext = &enabled_vulkan_1_3_features;
		features.timelineSemaphore = VK_TRUE;
		return features;
	}();
	vulkan_context->device_features.pNext = &enabled_vulkan_1_2_features;

	static const char *const h264_extensions[] = {
		VK_KHR_VIDEO_QUEUE_EXTENSION_NAME,
		VK_KHR_VIDEO_DECODE_QUEUE_EXTENSION_NAME,
		VK_KHR_VIDEO_DECODE_H264_EXTENSION_NAME,
	};
	static const char *const h265_extensions[] = {
		VK_KHR_VIDEO_QUEUE_EXTENSION_NAME,
		VK_KHR_VIDEO_DECODE_QUEUE_EXTENSION_NAME,
		VK_KHR_VIDEO_DECODE_H265_EXTENSION_NAME,
	};
	if (p_codec_id == AV_CODEC_ID_H264) {
		vulkan_context->enabled_dev_extensions = h264_extensions;
		vulkan_context->nb_enabled_dev_extensions = std::size(h264_extensions);
	} else {
		vulkan_context->enabled_dev_extensions = h265_extensions;
		vulkan_context->nb_enabled_dev_extensions = std::size(h265_extensions);
	}

	vulkan_context->qf[0].idx = device_info.decode_queue_family;
	vulkan_context->qf[0].num = 1;
	vulkan_context->qf[0].flags = VK_QUEUE_VIDEO_DECODE_BIT_KHR;
	vulkan_context->qf[0].video_caps = codec_operation;
	vulkan_context->qf[1].idx = device_info.auxiliary_queue_family;
	vulkan_context->qf[1].num = 1;
	vulkan_context->qf[1].flags = static_cast<VkQueueFlagBits>(VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT);
	// Include the renderer family in concurrent image sharing, but don't
	// offer its queues for FFmpeg compute/transfer submissions.
	vulkan_context->qf[2].idx = device_info.rendering_queue_family;
	vulkan_context->qf[2].num = 1;
	vulkan_context->qf[2].flags = VK_QUEUE_GRAPHICS_BIT;
	vulkan_context->nb_qf = 3;

	device_context->user_opaque = rd;
#if FF_API_VULKAN_SYNC_QUEUES
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
	vulkan_context->lock_queue = _ffmpeg_vulkan_queue_lock;
	vulkan_context->unlock_queue = _ffmpeg_vulkan_queue_unlock;
#ifdef _MSC_VER
#pragma warning(pop)
#else
#pragma GCC diagnostic pop
#endif
#endif

	const int init_result = av_hwdevice_ctx_init(vulkan_device_context);
	if (init_result < 0) {
		print_verbose(vformat("FFmpeg Vulkan bridge initialization failed: %s", ffmpeg_get_error_message(init_result)));
		av_buffer_unref(&vulkan_device_context);
		return false;
	}
	return true;
}

#endif

#ifdef FFMPEG_GPU_DECODE_ENABLED
static AVPixelFormat _vulkan_only_format(AVCodecContext *, const AVPixelFormat *p_formats) {
	for (; *p_formats != AV_PIX_FMT_NONE; p_formats++) {
		if (*p_formats == AV_PIX_FMT_VULKAN) {
			return *p_formats;
		}
	}
	return AV_PIX_FMT_NONE; // Never let failed hardware initialization retry in software.
}
#endif

#ifdef FFMPEG_GPU_TESTS_ENABLED
void VideoDecoder::test_vulkan_color_validation() {
	VulkanVideoConverter::enable_color_validation();
}

Dictionary VideoDecoder::test_vulkan_output_stats(RID p_texture) {
	return VulkanVideoConverter::get_statistics(p_texture);
}

Error VideoDecoder::test_vulkan_decode(const String &p_path) {
	if (!GLOBAL_GET("rendering/rendering_device/vulkan/video_decode").operator bool()) {
		return ERR_UNAVAILABLE;
	}
	// Import on the caller thread before launching workers. Both workers join
	// before their decoders (and all device/frame references) are destroyed.
	Ref<VideoDecoder> decoders[2];
	for (int i = 0; i < 2; i++) {
		Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ);
		if (file.is_null()) {
			return ERR_CANT_OPEN;
		}
		decoders[i] = memnew(VideoDecoder(file));
		VideoDecoder *decoder = decoders[i].ptr();
		// Use container metadata only: find_stream_info may invoke a software
		// decoder to probe frames before the Vulkan-only codec is installed.
		decoder->prepare_decoding(false);
		if (!decoder->video_stream || !decoder->_initialize_vulkan_device_context(decoder->video_stream->codecpar->codec_id)) {
			return ERR_UNAVAILABLE;
		}
		const AVCodec *codec = avcodec_find_decoder(decoder->video_stream->codecpar->codec_id);
		if (!codec) {
			return ERR_UNAVAILABLE;
		}
		decoder->video_codec_context = avcodec_alloc_context3(codec);
		AVCodecContext *context = decoder->video_codec_context;
		if (!context || avcodec_parameters_to_context(context, decoder->video_stream->codecpar) < 0) {
			return FAILED;
		}
		context->hw_device_ctx = av_buffer_ref(decoder->vulkan_device_context);
		context->get_format = _vulkan_only_format;
		context->thread_count = 1;
		if (!context->hw_device_ctx || avcodec_open2(context, codec, nullptr) < 0) {
			return FAILED;
		}
	}

	Error results[2] = { FAILED, FAILED };
	auto run = [&](int p_index) {
		VideoDecoder *decoder = decoders[p_index].ptr();
		AVCodecContext *context = decoder->video_codec_context;
		AVHWDeviceContext *device = reinterpret_cast<AVHWDeviceContext *>(decoder->vulkan_device_context->data);
		AVVulkanDeviceContext *vk = static_cast<AVVulkanDeviceContext *>(device->hwctx);
		PFN_vkWaitSemaphores wait_semaphores = reinterpret_cast<PFN_vkWaitSemaphores>(
				vk->get_proc_addr(vk->inst, "vkWaitSemaphores"));
		AVPacket *packet = av_packet_alloc();
		AVFrame *frame = av_frame_alloc();
		AVFrame *retained[4] = {};
		int frame_count = 0;
		bool valid = packet && frame && wait_semaphores;
		auto receive = [&]() {
			int result;
			while ((result = avcodec_receive_frame(context, frame)) >= 0) {
				if (frame->format != AV_PIX_FMT_VULKAN || !frame->hw_frames_ctx || !frame->data[0]) {
					return AVERROR(EINVAL);
				}
				AVHWFramesContext *frames = reinterpret_cast<AVHWFramesContext *>(frame->hw_frames_ctx->data);
				AVVulkanFramesContext *frames_vk = static_cast<AVVulkanFramesContext *>(frames->hwctx);
				AVVkFrame *gpu = reinterpret_cast<AVVkFrame *>(frame->data[0]);
				// Read metadata only; no mapping, download, swscale or CPU pixels.
				VkSemaphore semaphores[AV_NUM_DATA_POINTERS] = {};
				uint64_t values[AV_NUM_DATA_POINTERS] = {};
				frames_vk->lock_frame(frames, gpu);
				bool complete = gpu->img[0] != VK_NULL_HANDLE && gpu->sem[0] != VK_NULL_HANDLE;
				for (int plane = 0; plane < AV_NUM_DATA_POINTERS; plane++) {
					semaphores[plane] = gpu->sem[plane];
					values[plane] = gpu->sem_value[plane];
				}
				frames_vk->unlock_frame(frames, gpu);
				// The retained frame keeps the semaphore alive. Do not hold its
				// metadata lock while waiting for work already submitted.
				for (int plane = 0; complete && plane < AV_NUM_DATA_POINTERS && semaphores[plane]; plane++) {
					VkSemaphoreWaitInfo wait = {};
					wait.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
					wait.semaphoreCount = 1;
					wait.pSemaphores = &semaphores[plane];
					wait.pValues = &values[plane];
					complete = wait_semaphores(vk->act_dev, &wait, 5000000000ULL) == VK_SUCCESS;
				}
				if (!complete) {
					return AVERROR(EIO);
				}
				int slot = frame_count % 4;
				av_frame_free(&retained[slot]);
				retained[slot] = av_frame_clone(frame);
				if (!retained[slot]) {
					return AVERROR(ENOMEM);
				}
				frame_count++;
				av_frame_unref(frame);
			}
			return result;
		};
		for (int pass = 0; valid && pass < 3; pass++) {
			int start_count = frame_count;
			int read = 0;
			int packets = 0;
			while (valid && (read = av_read_frame(decoder->format_context, packet)) >= 0) {
				if (++packets > 100000) {
					valid = false;
				} else if (packet->stream_index == decoder->video_stream->index) {
					int sent = avcodec_send_packet(context, packet);
					int received = receive();
					valid = sent >= 0 && (received == AVERROR(EAGAIN) || received == AVERROR_EOF);
				}
				av_packet_unref(packet);
				if (pass == 2 && frame_count > start_count) {
					break; // Stop mid-stream without draining, then destroy the codec.
				}
			}
			if (pass == 2) {
				valid = valid && frame_count > start_count;
			} else if (valid && read == AVERROR_EOF) {
				int sent = avcodec_send_packet(context, nullptr);
				int received = receive();
				valid = sent >= 0 && received == AVERROR_EOF && frame_count > start_count;
			} else {
				valid = false;
			}
			if (valid && pass < 2) {
				avcodec_flush_buffers(context);
				valid = av_seek_frame(decoder->format_context, decoder->video_stream->index, 0, AVSEEK_FLAG_BACKWARD) >= 0;
			}
		}
		for (AVFrame *&held : retained) {
			av_frame_free(&held);
		}
		av_frame_free(&frame);
		av_packet_free(&packet);
		results[p_index] = valid ? OK : FAILED;
		print_line(vformat("VULKAN_DECODE_TEST decoder=%d frames=%d result=%s", p_index, frame_count, valid ? "PASS" : "FAIL"));
	};
	std::thread first(run, 0);
	std::thread second(run, 1);
	first.join();
	second.join();
	return results[0] == OK && results[1] == OK ? OK : FAILED;
}
#endif

bool is_hardware_pixel_format(AVPixelFormat p_fmt) {
	switch (p_fmt) {
		case AV_PIX_FMT_VDPAU:
		case AV_PIX_FMT_CUDA:
		case AV_PIX_FMT_VAAPI:
		case AV_PIX_FMT_DXVA2_VLD:
		case AV_PIX_FMT_QSV:
		case AV_PIX_FMT_VIDEOTOOLBOX:
		case AV_PIX_FMT_D3D11:
		case AV_PIX_FMT_D3D11VA_VLD:
		case AV_PIX_FMT_DRM_PRIME:
		case AV_PIX_FMT_OPENCL:
		case AV_PIX_FMT_MEDIACODEC:
		case AV_PIX_FMT_VULKAN:
		case AV_PIX_FMT_MMAL: {
			return true;
		}
		default: {
			return false;
		}
	}
	return false;
}

int VideoDecoder::_read_packet_callback(void *p_opaque, uint8_t *p_buf, int p_buf_size) {
	VideoDecoder *decoder = (VideoDecoder *)p_opaque;
	uint64_t read_bytes = decoder->video_file->get_buffer(p_buf, p_buf_size);
	return read_bytes != 0 ? read_bytes : AVERROR_EOF;
}

int64_t VideoDecoder::_stream_seek_callback(void *p_opaque, int64_t p_offset, int p_whence) {
	VideoDecoder *decoder = (VideoDecoder *)p_opaque;
	switch (p_whence) {
		case SEEK_CUR: {
			decoder->video_file->seek(decoder->video_file->get_position() + p_offset);
		} break;
		case SEEK_SET: {
			decoder->video_file->seek(p_offset);
		} break;
		case SEEK_END: {
			decoder->video_file->seek_end(p_offset);
		} break;
		case AVSEEK_SIZE: {
			return decoder->video_file->get_length();
		} break;
		default: {
			return -1;
		} break;
	}
	return decoder->video_file->get_position();
}

void VideoDecoder::prepare_decoding(bool p_probe_stream_info) {
	if (io_context) {
		avio_seek(io_context, 0, SEEK_SET);
	} else {
		const int context_buffer_size = 4096;
		unsigned char *context_buffer = (unsigned char *)av_malloc(context_buffer_size);
		ERR_FAIL_NULL_MSG(context_buffer, "Failed to allocate FFmpeg I/O buffer.");
		io_context = avio_alloc_context(context_buffer, context_buffer_size, 0, this, &VideoDecoder::_read_packet_callback, nullptr, &VideoDecoder::_stream_seek_callback);
		if (!io_context) {
			av_free(context_buffer);
			ERR_FAIL_MSG("Failed to allocate FFmpeg I/O context.");
		}
	}

	format_context = avformat_alloc_context();
	ERR_FAIL_NULL_MSG(format_context, "Failed to allocate FFmpeg format context.");
	format_context->pb = io_context;
	format_context->flags |= AVFMT_FLAG_GENPTS;
	format_context->video_codec = forced_video_codec;

	int open_input_res = avformat_open_input(&format_context, "dummy", nullptr, nullptr);
	input_opened = open_input_res >= 0;
	ERR_FAIL_COND_MSG(!input_opened, vformat("Error opening file or stream: %s", ffmpeg_get_error_message(open_input_res)));

	AVCodec *codec = nullptr;

	if (p_probe_stream_info) {
		int find_stream_info_result = avformat_find_stream_info(format_context, nullptr);
		ERR_FAIL_COND_MSG(find_stream_info_result < 0, vformat("Error finding stream info: %s", ffmpeg_get_error_message(find_stream_info_result)));
	}

	int stream_index = av_find_best_stream(format_context, AVMEDIA_TYPE_VIDEO, -1, -1, (const AVCodec **)&codec, 0);

	// The stream was found but FFmpeg has no decoder for its codec.
	// Re-probe ignoring decoder availability so we can name the codec and try a fallback.
	if (stream_index == AVERROR_DECODER_NOT_FOUND && format_context->video_codec == nullptr) {
		int raw_stream_index = av_find_best_stream(format_context, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
		if (raw_stream_index >= 0) {
			const AVCodecID unsupported_id = format_context->streams[raw_stream_index]->codecpar->codec_id;
			String fallback_name = _codec_id_to_preferred_decoder_name(unsupported_id);
			if (!fallback_name.is_empty()) {
				forced_video_codec = avcodec_find_decoder_by_name(fallback_name.utf8().get_data());
				if (forced_video_codec != nullptr) {
					print_line(vformat("No default decoder for '%s', retrying with '%s'.",
							avcodec_get_name(unsupported_id), fallback_name));
					avformat_close_input(&format_context);
					prepare_decoding(p_probe_stream_info);
					return;
				}
			}
			ERR_FAIL_MSG(vformat("No decoder found for '%s': codec not supported by this FFmpeg build.",
					avcodec_get_name(unsupported_id)));
		}
	}

	ERR_FAIL_COND_MSG(stream_index < 0, vformat("Couldn't find video stream: %s", ffmpeg_get_error_message(stream_index)));

	{
		if (format_context->video_codec == nullptr) {
			const AVCodecID codec_id = format_context->streams[stream_index]->codecpar->codec_id;
			String preferred_decoder_name = _codec_id_to_preferred_decoder_name(codec_id);
			if (!preferred_decoder_name.is_empty()) {
				forced_video_codec = avcodec_find_decoder_by_name(preferred_decoder_name.utf8().get_data());
				if (forced_video_codec != nullptr) {
					avformat_close_input(&format_context);
					prepare_decoding(p_probe_stream_info);
					return;
				}
			}
		}
	}

	video_stream = format_context->streams[stream_index];
	video_time_base_in_seconds = video_stream->time_base.num / (double)video_stream->time_base.den;
	if (video_stream->duration > 0) {
		duration = video_stream->duration * video_time_base_in_seconds * 1000.0;
	} else {
		duration = format_context->duration / (double)AV_TIME_BASE * 1000.0;
	}

	int audio_stream_index = av_find_best_stream(format_context, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
	if (audio_stream_index >= 0) {
		audio_stream = format_context->streams[audio_stream_index];
		audio_time_base_in_seconds = audio_stream->time_base.num / (double)audio_stream->time_base.den;
	}
}

Error VideoDecoder::recreate_codec_context() {
	if (video_stream == nullptr) {
		return ERR_BUG;
	}

#ifdef FFMPEG_GPU_DECODE_ENABLED
	av_buffer_unref(&vulkan_device_context);
#endif
	AVCodecParameters codec_params = *video_stream->codecpar;
	// YUV conversion needs rendering device
	bool has_rendering_device = RenderingServer::get_singleton()->get_rendering_device() != nullptr;
	bool is_yuv_pixel_fmt = codec_params.format == AVPixelFormat::AV_PIX_FMT_YUV420P || codec_params.format == AVPixelFormat::AV_PIX_FMT_YUVA420P;
	if (is_yuv_pixel_fmt && has_rendering_device) {
		frame_format = codec_params.format == AVPixelFormat::AV_PIX_FMT_YUV420P ? FFmpegFrameFormat::YUV420P : FFmpegFrameFormat::YUVA420P;
	} else {
		frame_format = FFmpegFrameFormat::RGBA8;
	}

	const AVCodec *decoder = forced_video_codec;
	if (!decoder) {
		decoder = avcodec_find_decoder(video_stream->codecpar->codec_id);
	}
	ERR_FAIL_NULL_V_MSG(decoder, ERR_CANT_CREATE, vformat("No decoder is available for codec %s.", avcodec_get_name(codec_params.codec_id)));
	if (video_codec_context != nullptr) {
		avcodec_free_context(&video_codec_context);
	}
	video_codec_context = avcodec_alloc_context3(decoder);
	video_codec_context->pkt_timebase = video_stream->time_base;

	ERR_FAIL_COND_V_MSG(video_codec_context == nullptr, FAILED, vformat("Couldn't allocate codec context: %s", decoder->name));

	int param_copy_result = avcodec_parameters_to_context(video_codec_context, &codec_params);

	ERR_FAIL_COND_V_MSG(param_copy_result < 0, FAILED, vformat("Couldn't copy codec parameters from %s: %s", decoder->name, ffmpeg_get_error_message(param_copy_result)));

	video_codec_context->thread_count = 0;

#ifdef FFMPEG_GPU_DECODE_ENABLED
	const bool video_decode_enabled = GLOBAL_GET("rendering/rendering_device/vulkan/video_decode").operator bool();
	vulkan_output_enabled = gpu_output_requested;
	if (vulkan_output_enabled && !video_decode_enabled) {
		gpu_failure_reason = "Vulkan Video device creation is disabled (video_decode=false).";
		if (decode_policy != DECODE_PREFER_GPU) {
			ERR_PRINT(gpu_failure_reason);
		}
		return ERR_UNAVAILABLE;
	}
	if (decode_policy != DECODE_SOFTWARE_ONLY && video_decode_enabled && _initialize_vulkan_device_context(codec_params.codec_id)) {
		if (vulkan_output_enabled) {
			video_codec_context->hw_device_ctx = av_buffer_ref(vulkan_device_context);
			ERR_FAIL_NULL_V(video_codec_context->hw_device_ctx, ERR_OUT_OF_MEMORY);
			video_codec_context->get_format = _vulkan_only_format;
			frame_format = FFmpegFrameFormat::VULKAN;
			print_verbose(decode_policy == DECODE_PREFER_GPU ?
							"FFmpeg Vulkan output enabled: GPU-resident frames; software restart fallback explicitly allowed." :
							"FFmpeg Vulkan output enabled: GPU-resident frames only; no CPU fallback.");
		} else {
			print_verbose("FFmpeg Vulkan device bridge initialized. GPU frame output is disabled.");
		}
	} else if (vulkan_output_enabled) {
		gpu_failure_reason = vformat("Vulkan decoding is unavailable for codec %s or the active RenderingDevice.", avcodec_get_name(codec_params.codec_id));
		if (decode_policy != DECODE_PREFER_GPU) {
			ERR_PRINT(gpu_failure_reason + " CPU fallback is not enabled.");
		}
		return ERR_UNAVAILABLE;
	}
#endif

	int open_codec_result = avcodec_open2(video_codec_context, decoder, nullptr);
	if (open_codec_result < 0 && vulkan_output_enabled && decode_policy == DECODE_PREFER_GPU) {
		gpu_failure_reason = vformat("Opening Vulkan decoder %s failed: %s", decoder->name, ffmpeg_get_error_message(open_codec_result));
		return FAILED;
	}
	ERR_FAIL_COND_V_MSG(open_codec_result < 0, FAILED, vformat("Error trying to open %s codec: %s", decoder->name, ffmpeg_get_error_message(open_codec_result)));

	print_line("Succesfully initialized video decoder:", decoder->long_name);

	ERR_FAIL_COND_V_MSG(video_codec_context == nullptr, ERR_CANT_CREATE, vformat("Error creating video codec context: Exhausted all available decoders for codec %s", avcodec_get_name(codec_params.codec_id)));

	if (!audio_stream) {
		return OK;
	}

	codec_params = *audio_stream->codecpar;
	const AVCodec *codec = avcodec_find_decoder(codec_params.codec_id);
	if (codec) {
		if (audio_codec_context != nullptr) {
			avcodec_free_context(&audio_codec_context);
		}
		audio_codec_context = avcodec_alloc_context3(codec);
		ERR_FAIL_COND_V_MSG(audio_codec_context == nullptr, FAILED, vformat("Couldn't allocate audio codec context: %s", codec->name));
		audio_codec_context->pkt_timebase = audio_stream->time_base;

		int param_copy_result = avcodec_parameters_to_context(audio_codec_context, audio_stream->codecpar);
		ERR_FAIL_COND_V_MSG(param_copy_result < 0, FAILED, vformat("Couldn't copy codec parameters from %s: %s", codec->name, ffmpeg_get_error_message(param_copy_result)));
		int open_codec_result = avcodec_open2(audio_codec_context, codec, nullptr);
		ERR_FAIL_COND_V_MSG(open_codec_result < 0, ERR_CANT_OPEN, vformat("Error trying to open %s codec: %s", codec->name, ffmpeg_get_error_message(open_codec_result)));
		has_audio = true;
	}
	return OK;
}

void VideoDecoder::_seek_command(double p_target_timestamp, bool p_preserve_preroll) {
	seek_in_progress.set();
	avcodec_flush_buffers(video_codec_context);
	av_seek_frame(format_context, video_stream->index, (long)(p_target_timestamp / video_time_base_in_seconds / 1000.0), AVSEEK_FLAG_BACKWARD);
	// No need to seek the audio stream separately since it is seeked automatically with the video stream
	// due to being in the same file
	if (has_audio) {
		avcodec_flush_buffers(audio_codec_context);
	}
	skip_output_until_time = p_target_timestamp;
	preserve_seek_preroll = p_preserve_preroll;
	seek_preroll_frame.unref();
	decoder_state = DecoderState::READY;
	skip_current_outputs.clear();
}

void VideoDecoder::_thread_func(void *userdata) {
	VideoDecoder *decoder = (VideoDecoder *)userdata;
	AVPacket *packet = av_packet_alloc();
	AVFrame *receive_frame = av_frame_alloc();
	if (packet == nullptr || receive_frame == nullptr) {
		av_packet_free(&packet);
		av_frame_free(&receive_frame);
		return;
	}

#ifdef GDEXTENSION
	String video_decoding_str = vformat("Video decoding %d", OS::get_singleton()->get_thread_caller_id());
#else
	String video_decoding_str = vformat("Video decoding %d", Thread::get_caller_id());
#endif
	CharString str = video_decoding_str.utf8();
	while (!decoder->thread_abort.is_set()) {
		switch (decoder->decoder_state) {
			case READY:
			case RUNNING: {
				decoder->decoded_frames_mutex->lock();
				bool needs_frame = decoder->decoded_frames.size() < MAX_PENDING_FRAMES;
				decoder->decoded_frames_mutex->unlock();
				if (needs_frame) {
					FrameMarkStart(video_decoding);
					decoder->_decode_next_frame(packet, receive_frame);
					FrameMarkEnd(video_decoding);
				} else {
					decoder->decoder_state = DecoderState::READY;
					OS::get_singleton()->delay_usec(1000);
				}
			} break;
			case END_OF_STREAM: {
				// While at the end of the stream, avoid attempting to read further as this comes with a non-negligible overhead.
				// A Seek() operation will trigger a state change, allowing decoding to potentially start again.
				OS::get_singleton()->delay_usec(50000);
			} break;
			case FAULTED: {
				OS::get_singleton()->delay_usec(50000);
			} break;
			default: {
				ERR_PRINT("Invalid decoder state");
			} break;
		}
		decoder->decoder_commands.flush_if_pending();
	}

	av_packet_free(&packet);
	av_frame_free(&receive_frame);

	if (decoder->decoder_state != DecoderState::FAULTED) {
		decoder->decoder_state = DecoderState::STOPPED;
	}
}

void VideoDecoder::_decode_next_frame(AVPacket *p_packet, AVFrame *p_receive_frame) {
	ZoneScopedN("Video decoder decode next frame");
	int read_frame_result = 0;

	if (p_packet->buf == nullptr) {
		read_frame_result = av_read_frame(format_context, p_packet);
	}

	if (read_frame_result >= 0) {
		decoder_state = DecoderState::RUNNING;

		bool unref_packet = true;

		if ((video_stream && p_packet->stream_index == video_stream->index) || (audio_stream && p_packet->stream_index == audio_stream->index)) {
			AVCodecContext *codec_ctx = video_codec_context;
			if (has_audio && p_packet->stream_index == audio_stream->index) {
				codec_ctx = audio_codec_context;
			}
			int send_packet_result = _send_packet(codec_ctx, p_receive_frame, p_packet);

			if (send_packet_result == -EAGAIN) {
				unref_packet = false;
			}
		}

		if (unref_packet) {
			av_packet_unref(p_packet);
		}
	} else if (read_frame_result == AVERROR_EOF) {
		_send_packet(video_codec_context, p_receive_frame, nullptr);
		if (has_audio) {
			_send_packet(audio_codec_context, p_receive_frame, nullptr);
		}
		seek_in_progress.clear();
		if (looping) {
			seek(0);
		} else {
			decoder_state = DecoderState::END_OF_STREAM;
		}
	} else if (read_frame_result == -EAGAIN) {
		decoder_state = DecoderState::READY;
		OS::get_singleton()->delay_usec(1000);
	} else {
		print_line(vformat("Failed to read data into avcodec packet: %s", ffmpeg_get_error_message(read_frame_result)));
	}
}

int VideoDecoder::_send_packet(AVCodecContext *p_codec_context, AVFrame *p_receive_frame, AVPacket *p_packet) {
	ZoneScopedN("Video/audio decoder send packet");
	ERR_FAIL_NULL_V(p_codec_context, AVERROR(EINVAL));
	// send the packet for decoding.
	int send_packet_result;
	{
		ZoneNamedN(__avcodec_send_packet, "avcodec_send_packet", true);
		send_packet_result = avcodec_send_packet(p_codec_context, p_packet);
	}
	// Note: EAGAIN can be returned if there's too many pending frames, which we have to read,
	// otherwise we would get stuck in an infinite loop.
	if (send_packet_result == 0 || send_packet_result == -EAGAIN) {
		if (p_codec_context == video_codec_context) {
			_read_decoded_frames(p_receive_frame);
		} else {
			_read_decoded_audio_frames(p_receive_frame);
		}
	} else if (p_codec_context == video_codec_context) {
		print_line(vformat("Failed to send avcodec packet: %s", ffmpeg_get_error_message(send_packet_result)));
		if (vulkan_output_enabled && send_packet_result != AVERROR_EOF) {
			decoder_state = DecoderState::FAULTED;
		}
	}

	return send_packet_result;
}

void VideoDecoder::_read_decoded_frames(AVFrame *p_received_frame) {
	Ref<Image> image;
	PackedByteArray unwrapped_frame;
	Ref<FFmpegFrame> pending_frame;
	while (true) {
		ZoneScopedN("Video decoder read decoded frame");
		int receive_frame_result = 0;
		bool emit_final_preroll = false;
		if (pending_frame.is_valid()) {
			av_frame_move_ref(p_received_frame, pending_frame->get_frame());
			pending_frame.unref();
		} else {
			receive_frame_result = avcodec_receive_frame(video_codec_context, p_received_frame);
		}
		if (receive_frame_result == AVERROR_EOF && preserve_seek_preroll && seek_preroll_frame.is_valid()) {
			// A frozen clock in the last frame's interval must still get an image.
			av_frame_move_ref(p_received_frame, seek_preroll_frame->get_frame());
			seek_preroll_frame.unref();
			preserve_seek_preroll = false;
			receive_frame_result = 0;
			emit_final_preroll = true;
		}

		if (receive_frame_result < 0) {
			if (receive_frame_result != -EAGAIN && receive_frame_result != AVERROR_EOF) {
				print_line(vformat("Failed to receive frame from avcodec: %s", ffmpeg_get_error_message(receive_frame_result)));
				if (vulkan_output_enabled) {
					decoder_state = DecoderState::FAULTED;
				}
			}

			break;
		}

		// use `best_effort_timestamp` as it can be more accurate if timestamps from the source file (pts) are broken.
		int64_t frame_timestamp = p_received_frame->best_effort_timestamp != AV_NOPTS_VALUE ? p_received_frame->best_effort_timestamp : p_received_frame->pts;
		double frame_time = (frame_timestamp - video_stream->start_time) * video_time_base_in_seconds * 1000.0;

		if (skip_current_outputs.is_set()) {
			continue;
		}
		if (!emit_final_preroll && skip_output_until_time > frame_time) {
			if (preserve_seek_preroll) {
				if (seek_preroll_frame.is_null()) {
					seek_preroll_frame.instantiate();
				}
				av_frame_unref(seek_preroll_frame->get_frame());
				av_frame_move_ref(seek_preroll_frame->get_frame(), p_received_frame);
				seek_preroll_time = frame_time;
			}
			continue;
		}
		// We have decoded through the seek target (or the final image at EOF).
		// Catch-up must not keep cancelling long-GOP preroll before this point.
		seek_in_progress.clear();
		if (preserve_seek_preroll) {
			if (seek_preroll_frame.is_valid()) {
				// Emit only the closest preceding image, then this future image.
				// This works for variable frame rates without guessing a duration.
				pending_frame.instantiate();
				av_frame_move_ref(pending_frame->get_frame(), p_received_frame);
				av_frame_move_ref(p_received_frame, seek_preroll_frame->get_frame());
				frame_time = seek_preroll_time;
				seek_preroll_frame.unref();
			}
			preserve_seek_preroll = false;
		}

		Ref<FFmpegFrame> frame;
		// copy data to a new AVFrame so that `receiveFrame` can be reused.
		frame.instantiate();
		if (frame->get_frame() == nullptr) {
			continue;
		}
		av_frame_move_ref(frame->get_frame(), p_received_frame);

		last_decoded_frame_time.set(frame_time);

#ifdef FFMPEG_GPU_DECODE_ENABLED
		if (vulkan_output_enabled) {
			if (frame->get_frame()->format != AV_PIX_FMT_VULKAN) {
				decoder_state = DecoderState::FAULTED;
				ERR_PRINT("Vulkan video output rejected a non-Vulkan frame. No CPU fallback is allowed.");
				break;
			}
			Ref<DecodedFrame> gpu_frame = memnew(DecodedFrame(frame_time, frame));
			decoded_frames_mutex->lock();
			if (!skip_current_outputs.is_set()) {
				decoded_frames.push_back(gpu_frame);
				decoded_backend.set(BACKEND_VULKAN);
			}
			decoded_frames_mutex->unlock();
			continue;
		}
#endif

		if (frame_format == FFmpegFrameFormat::YUV420P || frame_format == FFmpegFrameFormat::YUVA420P) {
			// Special path for YUV images
			Ref<DecodedFrame> yuv_frame = _unwrap_yuv_frame(frame_time, frame, frame_format);
			decoded_frames_mutex->lock();
			if (!skip_current_outputs.is_set()) {
				decoded_frames.push_back(yuv_frame);
				decoded_backend.set(BACKEND_SOFTWARE);
			}
			decoded_frames_mutex->unlock();
			continue;
		}

		// Note: this is the pixel format that the video texture expects internally
		frame = _ensure_frame_pixel_format(frame, AVPixelFormat::AV_PIX_FMT_RGBA);
		if (!frame.is_valid()) {
			continue;
		}

		ZoneNamedN(image_unwrap, "Image unwrap", true);
		// Unwrap the image
		int width = frame->get_frame()->width;
		int height = frame->get_frame()->height;
		{
			ZoneNamedN(image_unwrap_copy, "Image unwrap copy", true);

			AVFrame *rgba_frame = frame->get_frame();
			const int64_t row_bytes = (int64_t)width * 4;
			const bool total_bytes_overflow = row_bytes > 0 && height > std::numeric_limits<int64_t>::max() / row_bytes;
			const int64_t total_bytes = total_bytes_overflow ? 0 : row_bytes * height;
			const uintptr_t buffer_start = rgba_frame->buf[0] != nullptr ? (uintptr_t)rgba_frame->buf[0]->data : 0;
			const uintptr_t data_start = (uintptr_t)rgba_frame->data[0];
			const uintptr_t buffer_end = rgba_frame->buf[0] != nullptr && rgba_frame->buf[0]->size >= 0 && buffer_start <= std::numeric_limits<uintptr_t>::max() - (uintptr_t)rgba_frame->buf[0]->size ? buffer_start + (uintptr_t)rgba_frame->buf[0]->size : 0;
			const int64_t stride = rgba_frame->linesize[0];
			const bool stride_abs_overflow = stride == std::numeric_limits<int64_t>::min();
			const uintptr_t stride_abs = stride_abs_overflow ? 0 : (stride < 0 ? (uintptr_t)-stride : (uintptr_t)stride);
			const bool first_row_in_buffer = buffer_end != 0 && data_start >= buffer_start && data_start <= buffer_end && (uintptr_t)row_bytes <= buffer_end - data_start;
			const bool rows_in_buffer = first_row_in_buffer && (height <= 1 || (stride >= 0 ? stride_abs <= (buffer_end - data_start - (uintptr_t)row_bytes) / (height - 1) : stride_abs > 0 && (uintptr_t)(height - 1) <= (data_start - buffer_start) / stride_abs));

			if (rgba_frame->buf[0] == nullptr || rgba_frame->data[0] == nullptr || width <= 0 || height <= 0 || row_bytes <= 0 || total_bytes_overflow || total_bytes <= 0 || total_bytes > std::numeric_limits<int>::max() || stride_abs < (uintptr_t)row_bytes || stride_abs_overflow || !rows_in_buffer) {
				frame->do_return();
				continue;
			}
			unwrapped_frame.resize((int)total_bytes);
			uint8_t *unwrapped_frame_ptrw = unwrapped_frame.ptrw();
			{
				ZoneNamedN(image_unwrap_memcopy, "memcpy", true);
				for (int y = 0; y < height; y++) {
					memcpy(unwrapped_frame_ptrw, frame->get_frame()->data[0] + y * frame->get_frame()->linesize[0], width * 4);
					unwrapped_frame_ptrw += width * 4;
				}
			}
			unwrapped_frame.resize(width * height * 4);
			image = Image::create_from_data(width, height, false, Image::FORMAT_RGBA8, unwrapped_frame);
		}
#ifdef FFMPEG_MT_GPU_UPLOAD
		Ref<ImageTexture> tex;
		available_textures_mutex->lock();
		if (available_textures.size() > 0) {
			tex = available_textures[0];
			available_textures.pop_front();
		}
		available_textures_mutex->unlock();
		{
			ZoneNamedN(image_unwrap_gpu, "Image unwrap GPU upload", true);
			if (!tex.is_valid() || tex->get_size() != image->get_size() || tex->get_format() != image->get_format()) {
				ZoneNamedN(image_unwrap_gpu_texture_create, "Image unwrap GPU texture create", true);
				tex = ImageTexture::create_from_image(image);
			} else {
				ZoneNamedN(image_unwrap_gpu_texture_update, "Image unwrap GPU texture update", true);
				tex->update(image);
			}
		}
		decoded_frames_mutex->lock();
		decoded_frames.push_back(memnew(DecodedFrame(frame_time, tex)));
		decoded_backend.set(BACKEND_SOFTWARE);
		decoded_frames_mutex->unlock();
#else
		decoded_frames_mutex->lock();
		if (!skip_current_outputs.is_set()) {
			decoded_frames.push_back(memnew(DecodedFrame(frame_time, image)));
			decoded_backend.set(BACKEND_SOFTWARE);
		}
		decoded_frames_mutex->unlock();
#endif
	}
}

void VideoDecoder::_read_decoded_audio_frames(AVFrame *p_received_frame) {
	Vector<uint8_t> unwrapped_frame;
	while (true) {
		ZoneScopedN("Audio decoder read decoded frame");
		int receive_frame_result = avcodec_receive_frame(audio_codec_context, p_received_frame);

		if (receive_frame_result < 0) {
			if (receive_frame_result != -EAGAIN && receive_frame_result != AVERROR_EOF) {
				print_line(vformat("Failed to receive frame from avcodec: %s", ffmpeg_get_error_message(receive_frame_result)));
			}

			break;
		}

		// use `best_effort_timestamp` as it can be more accurate if timestamps from the source file (pts) are broken.
		int64_t frame_timestamp = p_received_frame->best_effort_timestamp != AV_NOPTS_VALUE ? p_received_frame->best_effort_timestamp : p_received_frame->pts;
		double frame_time = (frame_timestamp - audio_stream->start_time) * audio_time_base_in_seconds * 1000.0;

		if (skip_output_until_time > frame_time || skip_current_outputs.is_set()) {
			continue;
		}

		AVFrame *frame = _ensure_frame_audio_format(p_received_frame, AV_SAMPLE_FMT_FLT);

		if (!frame) {
			av_frame_unref(p_received_frame);
			return;
		}

		ERR_FAIL_COND_MSG(av_sample_fmt_is_planar((AVSampleFormat)frame->format), "Audio format should never be planar, bug?");

		int data_size = av_samples_get_buffer_size(nullptr, frame->ch_layout.nb_channels, frame->nb_samples, (AVSampleFormat)frame->format, 0);
		if (data_size <= 0 || frame->data[0] == nullptr) {
			if (frame != p_received_frame) {
				av_frame_free(&frame);
			}
			av_frame_unref(p_received_frame);
			return;
		}
		Ref<DecodedAudioFrame> audio_frame = memnew(DecodedAudioFrame(frame_time));
		audio_frame->sample_data.resize(data_size / sizeof(float));
		memcpy(audio_frame->sample_data.ptrw(), frame->data[0], data_size);
		audio_buffer_mutex->lock();
		if (!skip_current_outputs.is_set()) {
			decoded_audio_frames.push_back(audio_frame);
		}
		audio_buffer_mutex->unlock();

		av_frame_unref(p_received_frame);
		if (frame != p_received_frame) {
			av_frame_free(&frame);
		}
	}
}

void VideoDecoder::_scaler_frame_return(Ref<FFmpegFrame> p_scaler_frame) {
	scaler_frames.push_back(p_scaler_frame);
}

Ref<FFmpegFrame> VideoDecoder::_ensure_frame_pixel_format(Ref<FFmpegFrame> p_frame, int p_target_pixel_format) {
	ZoneScopedN("Video decoder rescale");
	if (!p_frame.is_valid() || p_frame->get_frame() == nullptr) {
		return Ref<FFmpegFrame>();
	}

	if (p_frame->get_frame()->format == (AVPixelFormat)p_target_pixel_format) {
		return p_frame;
	}

	int width = p_frame->get_frame()->width;
	int height = p_frame->get_frame()->height;

	sws_context = sws_getCachedContext(
			sws_context,
			width, height, (AVPixelFormat)p_frame->get_frame()->format,
			width, height, (AVPixelFormat)p_target_pixel_format,
			1, nullptr, nullptr, nullptr);
	if (sws_context == nullptr) {
		print_line("Failed to create SWS context");
		p_frame->do_return();
		return Ref<FFmpegFrame>();
	}

	Ref<FFmpegFrame> scaler_frame;
	{
		if (scaler_frames.size() > 0) {
			scaler_frame = scaler_frames.front()->get();
			scaler_frames.pop_front();
		}
	}

	if (!scaler_frame.is_valid()) {
		scaler_frame.instantiate();
		if (scaler_frame->get_frame() == nullptr) {
			return Ref<FFmpegFrame>();
		}
		scaler_frame->connect("return_frame", callable_mp(this, &VideoDecoder::_scaler_frame_return));
	}

	// (re)initialize the scaler frame if needed.
	if (scaler_frame->get_frame()->format != p_target_pixel_format || scaler_frame->get_frame()->width != width || scaler_frame->get_frame()->height != height) {
		av_frame_unref(scaler_frame->get_frame());

		// Note: this field determines the scaler's output pix format.
		scaler_frame->get_frame()->format = p_target_pixel_format;
		scaler_frame->get_frame()->width = width;
		scaler_frame->get_frame()->height = height;

		int get_buffer_result = av_frame_get_buffer(scaler_frame->get_frame(), 0);

		if (get_buffer_result < 0) {
			print_line("Failed to allocate SWS frame buffer:", ffmpeg_get_error_message(get_buffer_result));
			p_frame->do_return();
			return Ref<FFmpegFrame>();
		}
	}

	int scaler_result = sws_scale(
			sws_context,
			p_frame->get_frame()->data, p_frame->get_frame()->linesize, 0, height,
			scaler_frame->get_frame()->data, scaler_frame->get_frame()->linesize);

	// return the original frame regardless of the scaler result.
	p_frame->do_return();

	if (scaler_result < 0) {
		print_line("Failed to scale frame:", ffmpeg_get_error_message(scaler_result));
		return Ref<FFmpegFrame>();
	}

	return scaler_frame;
}

Ref<DecodedFrame> VideoDecoder::_unwrap_yuv_frame(double p_frame_time, Ref<FFmpegFrame> p_frame, FFmpegFrameFormat p_out_format) {
	PackedByteArray temp_frame_storage;
	Ref<DecodedFrame> out_frame = memnew(DecodedFrame(p_frame_time, Ref<Image>()));
	const int frame_plane_count = p_out_format == FFmpegFrameFormat::YUV420P ? 3 : 4;
	for (size_t plane_i = 0; plane_i < frame_plane_count; plane_i++) {
		ZoneNamedN(yuv_image_unwrap_copy, "YUV Image unwrap copy", true);

		int width = p_frame->get_frame()->width;
		int height = p_frame->get_frame()->height;

		if (plane_i > 0 && plane_i < 3) {
			width = Math::ceil(width / 2.0f);
			height = Math::ceil(height / 2.0f);
		}

		const int plane_size = p_frame->get_frame()->linesize[plane_i] * height;
		temp_frame_storage.resize(plane_size);
		uint8_t *unwrapped_frame_ptrw = temp_frame_storage.ptrw();
		{
			ZoneNamedN(yuv_image_unwrap_memcopy, "YUV memcpy", true);
			for (int y = 0; y < height; y++) {
				memcpy(unwrapped_frame_ptrw, p_frame->get_frame()->data[plane_i] + y * p_frame->get_frame()->linesize[plane_i], width);
				unwrapped_frame_ptrw += width;
			}
		}
		temp_frame_storage.resize(width * height);
		out_frame->set_yuv_image_plane(plane_i, Image::create_from_data(width, height, false, Image::FORMAT_R8, temp_frame_storage));
	}

	out_frame->set_format(p_out_format);

	return out_frame;
}

AVFrame *VideoDecoder::_ensure_frame_audio_format(AVFrame *p_frame, int p_target_audio_format) {
	ZoneScopedN("Audio decoder rescale");
	if (p_frame->format == p_target_audio_format) {
		return p_frame;
	}

	int obtain_swr_ctx_result = swr_alloc_set_opts2(
			&swr_context,
			&audio_codec_context->ch_layout, (AVSampleFormat)p_target_audio_format, audio_codec_context->sample_rate,
			&audio_codec_context->ch_layout, audio_codec_context->sample_fmt, audio_codec_context->sample_rate,
			0, nullptr);

	if (obtain_swr_ctx_result < 0) {
		print_line("Failed to obtain SWR context");
		return nullptr;
	}
	AVFrame *out_frame;

	out_frame = av_frame_alloc();
	if (out_frame == nullptr) {
		print_line("Failed to allocate SWR frame");
		return nullptr;
	}
	out_frame->format = p_target_audio_format;
	out_frame->ch_layout = audio_codec_context->ch_layout;
	out_frame->sample_rate = audio_codec_context->sample_rate;
	out_frame->nb_samples = p_frame->nb_samples;

	int get_buffer_result = av_frame_get_buffer(out_frame, 0);

	if (get_buffer_result < 0) {
		print_line("Failed to allocate SWR frame buffer:", ffmpeg_get_error_message(get_buffer_result));
		av_frame_free(&out_frame);
		return nullptr;
	}

	int converter_result = swr_convert_frame(swr_context, out_frame, p_frame);

	if (converter_result < 0) {
		print_line("Failed to convert audio frame:", ffmpeg_get_error_message(converter_result));
		av_frame_free(&out_frame);
		return nullptr;
	}

	return out_frame;
}

String VideoDecoder::_codec_id_to_preferred_decoder_name(int p_codec_id) const {
	switch (p_codec_id) {
		case AVCodecID::AV_CODEC_ID_VP8: {
			return "libvpx";
		} break;
		case AVCodecID::AV_CODEC_ID_VP9: {
			return "libvpx-vp9";
		} break;
		case AVCodecID::AV_CODEC_ID_HEVC: {
			return "hevc";
		} break;
		case AVCodecID::AV_CODEC_ID_AV1: {
			return "libaom-av1";
		} break;
		default: {
		} break;
	}
	return String();
}

void VideoDecoder::seek(double p_time, bool p_wait, bool p_preserve_preroll) {
	seek_in_progress.set();
	decoded_frames_mutex->lock();
	audio_buffer_mutex->lock();

	decoded_frames.clear();
	decoded_audio_frames.clear();

	last_decoded_frame_time.set(p_time);
	skip_current_outputs.set();
	decoded_frames_mutex->unlock();
	audio_buffer_mutex->unlock();
	if (p_wait) {
		decoder_commands.push_and_sync(this, &VideoDecoder::_seek_command, p_time, p_preserve_preroll);
	} else {
		decoder_commands.push(this, &VideoDecoder::_seek_command, p_time, p_preserve_preroll);
	}
}

void VideoDecoder::start_decoding() {
	ERR_FAIL_COND_MSG(thread != nullptr, "Cannot start decoding once already started");
#if defined(FFMPEG_ENGINE_MODULE) && !defined(FFMPEG_GPU_DECODE_ENABLED)
	if (gpu_output_requested) {
		decoder_state = DecoderState::FAULTED;
		gpu_failure_reason = "This engine was built without Vulkan Video decoding support.";
		if (decode_policy != DECODE_PREFER_GPU) {
			ERR_PRINT(gpu_failure_reason + " CPU fallback is not enabled.");
		}
		return;
	}
#endif
	if (format_context == nullptr) {
		bool probe_stream_info = !gpu_output_requested;
		prepare_decoding(probe_stream_info);
		Error codec_context_create_error = recreate_codec_context();

		if (video_stream == nullptr || codec_context_create_error != OK) {
			decoder_state = DecoderState::FAULTED;
			return;
		}
	}

	thread = memnew(std::thread(_thread_func, this));
}

void VideoDecoder::return_frames(Vector<Ref<DecodedFrame>> p_frames) {
	for (Ref<DecodedFrame> frame : p_frames) {
		return_frame(frame);
	}
}

void VideoDecoder::return_frame(Ref<DecodedFrame> p_frame) {
#ifdef FFMPEG_GPU_DECODE_ENABLED
	if (p_frame->get_format() == FFmpegFrameFormat::VULKAN) {
		return; // AVFrame references are released only by their actual owners.
	}
#endif
	available_textures_mutex->lock();
	available_textures.push_back(p_frame->get_texture());
	available_textures_mutex->unlock();
}

Vector<Ref<DecodedFrame>> VideoDecoder::get_decoded_frames() {
	Vector<Ref<DecodedFrame>> frames;
	decoded_frames_mutex->lock();
	frames = decoded_frames.duplicate();
	decoded_frames.clear();
	decoded_frames_mutex->unlock();
	return frames;
}

Vector<Ref<DecodedAudioFrame>> VideoDecoder::get_decoded_audio_frames() {
	audio_buffer_mutex->lock();
	Vector<Ref<DecodedAudioFrame>> frames = decoded_audio_frames.duplicate();
	decoded_audio_frames.clear();
	audio_buffer_mutex->unlock();
	return frames;
}

VideoDecoder::DecoderState VideoDecoder::get_decoder_state() const {
	return decoder_state;
}

double VideoDecoder::get_last_decoded_frame_time() const {
	return last_decoded_frame_time.get();
}

bool VideoDecoder::is_running() const {
	return decoder_state == DecoderState::RUNNING;
}

double VideoDecoder::get_duration() const {
	return duration;
}

Vector2i VideoDecoder::get_size() const {
	if (video_codec_context) {
		return Vector2i(video_codec_context->width, video_codec_context->height);
	}
	return Vector2i();
}

int VideoDecoder::get_audio_mix_rate() const {
	if (audio_stream) {
		return audio_codec_context->sample_rate;
	}
	return 0;
}

int VideoDecoder::get_audio_channel_count() const {
	if (audio_stream) {
		return audio_codec_context->ch_layout.nb_channels;
	}
	return 0;
}

VideoDecoder::VideoDecoder(Ref<FileAccess> p_file, DecodePolicy p_policy) {
	video_file = p_file;
	decode_policy = p_policy;
#ifndef GDEXTENSION
	gpu_output_requested = decode_policy == DECODE_PREFER_GPU || decode_policy == DECODE_REQUIRE_GPU ||
			(decode_policy == DECODE_DEFAULT && ProjectSettings::get_singleton()->get_setting("rendering/rendering_device/vulkan/video_decode_output", false).operator bool());
#endif
	available_textures_mutex.instantiate();
	hw_transfer_frames_mutex.instantiate();
	scaler_frames_mutex.instantiate();
	decoded_frames_mutex.instantiate();
	audio_buffer_mutex.instantiate();
}

VideoDecoder::~VideoDecoder() {
	if (thread != nullptr) {
		thread_abort.set_to(true);
		thread->join();
		memdelete(thread);
	}

	if (format_context != nullptr && input_opened) {
		avformat_close_input(&format_context);
	}

	if (video_codec_context != nullptr) {
		avcodec_free_context(&video_codec_context);
	}
#ifdef FFMPEG_GPU_DECODE_ENABLED
	av_buffer_unref(&vulkan_device_context);
#endif

	if (audio_codec_context != nullptr) {
		avcodec_free_context(&audio_codec_context);
	}

	if (sws_context != nullptr) {
		sws_freeContext(sws_context);
	}

	if (swr_context != nullptr) {
		swr_free(&swr_context);
	}

	if (io_context != nullptr) {
		av_free(io_context->buffer);
		avio_context_free(&io_context);
	}
}

#ifdef FFMPEG_GPU_DECODE_ENABLED
DecodedFrame::DecodedFrame(double p_time, Ref<FFmpegFrame> p_frame) :
		time(p_time), format(FFmpegFrameFormat::VULKAN), vulkan_frame(p_frame) {
}
#endif

DecodedFrame::DecodedFrame(double p_time, Ref<ImageTexture> p_texture) {
	time = p_time;
	texture = p_texture;
}

DecodedFrame::DecodedFrame(double p_time, Ref<Image> p_image) {
	time = p_time;
	image = p_image;
	format = FFmpegFrameFormat::RGBA8;
}

Ref<ImageTexture> DecodedFrame::get_texture() const { return texture; }

void DecodedFrame::set_texture(const Ref<ImageTexture> &p_texture) { texture = p_texture; }

double DecodedFrame::get_time() const { return time; }

void DecodedFrame::set_time(double p_time) { time = p_time; }

void DecodedFrame::set_yuv_image_plane(int p_plane_idx, Ref<Image> p_image) {
	ERR_FAIL_INDEX((size_t)p_plane_idx, std::size(yuv_images));
	yuv_images[p_plane_idx] = p_image;
}

Ref<Image> DecodedFrame::get_yuv_image_plane(int p_plane_idx) const {
	ERR_FAIL_INDEX_V((size_t)p_plane_idx, std::size(yuv_images), Ref<Image>());
	return yuv_images[p_plane_idx];
}

double DecodedAudioFrame::get_time() const {
	return time;
}

PackedFloat32Array DecodedAudioFrame::get_sample_data() const {
	return sample_data;
}
String ffmpeg_get_error_message(int p_error_code) {
	const uint64_t buffer_size = 256;
	Vector<char> buffer;
	buffer.resize(buffer_size);

	int str_error_code = av_strerror(p_error_code, buffer.ptrw(), buffer.size());

	if (str_error_code < 0) {
		return vformat("%d (av_strerror failed with code %d)", p_error_code, str_error_code);
	}

	return String::utf8(buffer.ptr());
}
