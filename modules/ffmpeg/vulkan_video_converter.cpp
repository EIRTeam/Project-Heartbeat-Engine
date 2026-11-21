#if defined(FFMPEG_GPU_DECODE_ENABLED) && !defined(GDEXTENSION)
#include "vulkan_video_converter.h"
#include "core/object/callable_mp.h"
#include "drivers/vulkan/rendering_device_driver_vulkan.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/rendering_server.h"
#include "vulkan_yuv_to_rgb.glsl.gen.h"
#ifdef FFMPEG_GPU_TESTS_ENABLED
#include "vulkan_validate_rgb.glsl.gen.h"
#endif
extern "C" {
#include "libavutil/hwcontext_vulkan.h"
}

namespace {
using RD = RenderingDevice;
bool supports_output_usage(VkImageUsageFlags p_usage) {
	return (p_usage & VK_IMAGE_USAGE_SAMPLED_BIT) && !(p_usage & VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR);
}
#ifdef FFMPEG_GPU_TESTS_ENABLED
SafeNumeric<uint64_t> submitted_jobs;
SafeNumeric<uint64_t> completed_jobs;
SafeNumeric<uint64_t> color_checked;
SafeNumeric<uint64_t> color_failed;
SafeFlag validate_colors;
struct TextureStatistics {
	uint64_t submitted = 0;
	uint64_t completed = 0;
	uint64_t color_checked = 0;
	uint64_t color_failed = 0;
	int64_t last_pts = AV_NOPTS_VALUE;
};
Mutex statistics_mutex;
HashMap<RID, TextureStatistics> texture_statistics;
#endif

struct ConversionState : RefCounted {
	VkDevice device = VK_NULL_HANDLE;
	VkDescriptorSetLayout descriptors = VK_NULL_HANDLE;
	VkPipelineLayout layout = VK_NULL_HANDLE;
	VkPipeline pipeline = VK_NULL_HANDLE;
	VkSampler sampler = VK_NULL_HANDLE;
	VkFormat logged_format = VK_FORMAT_UNDEFINED;
#ifdef FFMPEG_GPU_TESTS_ENABLED
	VkPhysicalDevice physical_device = VK_NULL_HANDLE;
	VkPipeline validation_pipeline = VK_NULL_HANDLE;
#endif
	struct Params {
		float range[4];
		float coefficients[4];
		int32_t size[2];
	};

	bool create_pipeline(RD *p_rd, const char *p_source, VkPipeline &r_pipeline) {
		String error;
		Vector<uint8_t> code = p_rd->shader_compile_spirv_from_source(RD::SHADER_STAGE_COMPUTE,
				p_source + 10, RD::SHADER_LANGUAGE_GLSL, &error);
		if (code.is_empty()) {
			ERR_PRINT(error);
			return false;
		}
		VkShaderModuleCreateInfo module_info = {};
		module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		module_info.codeSize = code.size();
		module_info.pCode = reinterpret_cast<const uint32_t *>(code.ptr());
		VkShaderModule module = VK_NULL_HANDLE;
		if (vkCreateShaderModule(device, &module_info, nullptr, &module) != VK_SUCCESS) {
			return false;
		}
		VkComputePipelineCreateInfo pipeline_info = {};
		pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		pipeline_info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		pipeline_info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		pipeline_info.stage.module = module;
		pipeline_info.stage.pName = "main";
		pipeline_info.layout = layout;
		VkResult result = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &r_pipeline);
		vkDestroyShaderModule(device, module, nullptr);
		return result == VK_SUCCESS;
	}

	bool initialize(RD *p_rd) {
		RD::VulkanVideoDecodeDeviceInfo info;
		if (!p_rd->get_vulkan_video_decode_device_info(info)) {
			return false;
		}
		device = reinterpret_cast<VkDevice>(info.logical_device);
#ifdef FFMPEG_GPU_TESTS_ENABLED
		physical_device = reinterpret_cast<VkPhysicalDevice>(info.physical_device);
		const uint32_t binding_count = 4;
#else
		const uint32_t binding_count = 3;
#endif
		VkDescriptorSetLayoutBinding bindings[binding_count] = {};
		for (uint32_t i = 0; i < binding_count; i++) {
			bindings[i].binding = i;
			bindings[i].descriptorCount = 1;
			bindings[i].descriptorType = i < 2 ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER : (i == 2 ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
			bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		}
		VkDescriptorSetLayoutCreateInfo set_info = {};
		set_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		set_info.bindingCount = binding_count;
		set_info.pBindings = bindings;
		if (vkCreateDescriptorSetLayout(device, &set_info, nullptr, &descriptors) != VK_SUCCESS) {
			return false;
		}
		VkPushConstantRange push = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Params) };
		VkPipelineLayoutCreateInfo layout_info = {};
		layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		layout_info.setLayoutCount = 1;
		layout_info.pSetLayouts = &descriptors;
		layout_info.pushConstantRangeCount = 1;
		layout_info.pPushConstantRanges = &push;
		if (vkCreatePipelineLayout(device, &layout_info, nullptr, &layout) != VK_SUCCESS) {
			return false;
		}
		if (!create_pipeline(p_rd, vulkan_yuv_to_rgb_shader_glsl, pipeline)) {
			return false;
		}
#ifdef FFMPEG_GPU_TESTS_ENABLED
		if (validate_colors.is_set() && !create_pipeline(p_rd, vulkan_validate_rgb_shader_glsl, validation_pipeline)) {
			return false;
		}
#endif
		VkSamplerCreateInfo sampler_info = {};
		sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sampler_info.magFilter = VK_FILTER_NEAREST;
		sampler_info.minFilter = VK_FILTER_NEAREST;
		sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		return vkCreateSampler(device, &sampler_info, nullptr, &sampler) == VK_SUCCESS;
	}

	~ConversionState() {
#ifdef FFMPEG_GPU_TESTS_ENABLED
		if (validation_pipeline) {
			vkDestroyPipeline(device, validation_pipeline, nullptr);
		}
#endif
		if (sampler) {
			vkDestroySampler(device, sampler, nullptr);
		}
		if (pipeline) {
			vkDestroyPipeline(device, pipeline, nullptr);
		}
		if (layout) {
			vkDestroyPipelineLayout(device, layout, nullptr);
		}
		if (descriptors) {
			vkDestroyDescriptorSetLayout(device, descriptors, nullptr);
		}
	}
};

struct ConversionJob : RefCounted {
	Ref<RefCounted> owner;
	Ref<ConversionState> state;
	Ref<FFmpegFrame> frame;
	SafeFlag *failed = nullptr;
	VkImageView planes[2] = {};
	VkDescriptorPool pool = VK_NULL_HANDLE;
	VkDescriptorSet set = VK_NULL_HANDLE;
	ConversionState::Params params = {};
	bool recorded = false;
	RID output_rid;
#ifdef FFMPEG_GPU_TESTS_ENABLED
	VkImage output_image = VK_NULL_HANDLE;
	VkBuffer validation_buffer = VK_NULL_HANDLE;
	VkDeviceMemory validation_memory = VK_NULL_HANDLE;
	void *validation_result = nullptr;

	bool initialize_color_check() {
		VkBufferCreateInfo buffer = {};
		buffer.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer.size = 2 * sizeof(uint32_t);
		buffer.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		if (vkCreateBuffer(state->device, &buffer, nullptr, &validation_buffer) != VK_SUCCESS) {
			return false;
		}
		VkMemoryRequirements requirements;
		vkGetBufferMemoryRequirements(state->device, validation_buffer, &requirements);
		VkPhysicalDeviceMemoryProperties properties;
		vkGetPhysicalDeviceMemoryProperties(state->physical_device, &properties);
		uint32_t memory_type = UINT32_MAX;
		for (uint32_t i = 0; i < properties.memoryTypeCount; i++) {
			VkMemoryPropertyFlags needed = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			if ((requirements.memoryTypeBits & (1u << i)) && (properties.memoryTypes[i].propertyFlags & needed) == needed) {
				memory_type = i;
				break;
			}
		}
		if (memory_type == UINT32_MAX) {
			return false;
		}
		VkMemoryAllocateInfo allocate = {};
		allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocate.allocationSize = requirements.size;
		allocate.memoryTypeIndex = memory_type;
		if (vkAllocateMemory(state->device, &allocate, nullptr, &validation_memory) != VK_SUCCESS ||
				vkBindBufferMemory(state->device, validation_buffer, validation_memory, 0) != VK_SUCCESS ||
				vkMapMemory(state->device, validation_memory, 0, VK_WHOLE_SIZE, 0, &validation_result) != VK_SUCCESS) {
			return false;
		}
		// Diagnostic counters only. Image pixels never reach CPU memory.
		memset(validation_result, 0, 2 * sizeof(uint32_t));
		VkDescriptorBufferInfo info = { validation_buffer, 0, 2 * sizeof(uint32_t) };
		VkWriteDescriptorSet write = {};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = set;
		write.dstBinding = 3;
		write.descriptorCount = 1;
		write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		write.pBufferInfo = &info;
		vkUpdateDescriptorSets(state->device, 1, &write, 0, nullptr);
		return true;
	}
#endif

	bool initialize(VkImageView p_output_view) {
		AVFrame *source = frame->get_frame();
		if (source->format != AV_PIX_FMT_VULKAN || !source->hw_frames_ctx || !source->data[0] ||
				source->crop_top || source->crop_bottom || source->crop_left || source->crop_right ||
				(source->flags & AV_FRAME_FLAG_INTERLACED)) {
			return false;
		}
		AVHWFramesContext *frames = reinterpret_cast<AVHWFramesContext *>(source->hw_frames_ctx->data);
		AVVulkanFramesContext *vk_frames = static_cast<AVVulkanFramesContext *>(frames->hwctx);
		// COINCIDE-mode images are also decoder references. Publishing their
		// return values before an entire renderer submission runs can create
		// a decode <-> render queue cycle. Support DISTINCT outputs only.
		if (vk_frames->usage & VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR) {
			ERR_PRINT("Vulkan video output requires distinct decoder reference/output images; coinciding DPB/output is unsupported. No CPU fallback is allowed.");
			return false;
		}
		AVVkFrame *gpu = reinterpret_cast<AVVkFrame *>(source->data[0]);
		bool ten_bit = vk_frames->format[0] == VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16;
		if ((!ten_bit && vk_frames->format[0] != VK_FORMAT_G8_B8R8_2PLANE_420_UNORM) ||
				vk_frames->nb_layers != 1 || !supports_output_usage(vk_frames->usage) ||
				gpu->img[1] || gpu->queue_family[0] != VK_QUEUE_FAMILY_IGNORED) {
			return false;
		}
		// All exposed FFmpeg and renderer families are in the concurrent image
		// sharing set. Exclusive/external ownership is rejected, never guessed.
		for (int i = 0; i < 2; i++) {
			VkImageViewUsageCreateInfo usage = {};
			usage.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO;
			usage.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
			VkImageViewCreateInfo view = {};
			view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			view.pNext = &usage;
			view.image = gpu->img[0];
			view.viewType = VK_IMAGE_VIEW_TYPE_2D;
			view.format = ten_bit ? (i == 0 ? VK_FORMAT_R16_UNORM : VK_FORMAT_R16G16_UNORM) : (i == 0 ? VK_FORMAT_R8_UNORM : VK_FORMAT_R8G8_UNORM);
			view.subresourceRange.aspectMask = i == 0 ? VK_IMAGE_ASPECT_PLANE_0_BIT : VK_IMAGE_ASPECT_PLANE_1_BIT;
			view.subresourceRange.levelCount = 1;
			view.subresourceRange.layerCount = 1;
			if (vkCreateImageView(state->device, &view, nullptr, &planes[i]) != VK_SUCCESS) {
				return false;
			}
		}
		VkDescriptorPoolSize sizes[] = {
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
#ifdef FFMPEG_GPU_TESTS_ENABLED
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 },
#endif
		};
		VkDescriptorPoolCreateInfo pool_info = {};
		pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_info.maxSets = 1;
		pool_info.poolSizeCount = sizeof(sizes) / sizeof(sizes[0]);
		pool_info.pPoolSizes = sizes;
		if (vkCreateDescriptorPool(state->device, &pool_info, nullptr, &pool) != VK_SUCCESS) {
			return false;
		}
		VkDescriptorSetAllocateInfo allocate = {};
		allocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocate.descriptorPool = pool;
		allocate.descriptorSetCount = 1;
		allocate.pSetLayouts = &state->descriptors;
		if (vkAllocateDescriptorSets(state->device, &allocate, &set) != VK_SUCCESS) {
			return false;
		}
		VkDescriptorImageInfo images[3] = {
			{ state->sampler, planes[0], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
			{ state->sampler, planes[1], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
			{ VK_NULL_HANDLE, p_output_view, VK_IMAGE_LAYOUT_GENERAL },
		};
		VkWriteDescriptorSet writes[3] = {};
		for (int i = 0; i < 3; i++) {
			writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[i].dstSet = set;
			writes[i].dstBinding = i;
			writes[i].descriptorCount = 1;
			writes[i].descriptorType = i == 2 ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writes[i].pImageInfo = &images[i];
		}
		vkUpdateDescriptorSets(state->device, 3, writes, 0, nullptr);
#ifdef FFMPEG_GPU_TESTS_ENABLED
		if (state->validation_pipeline && !initialize_color_check()) {
			return false;
		}
#endif
		float kr = 0.299f;
		float kb = 0.114f;
		switch (source->colorspace) {
			case AVCOL_SPC_BT709:
				kr = 0.2126f;
				kb = 0.0722f;
				break;
			case AVCOL_SPC_UNSPECIFIED:
				if (source->width >= 1280 || source->height > 576) {
					kr = 0.2126f;
					kb = 0.0722f;
				}
				break;
			case AVCOL_SPC_BT470BG:
			case AVCOL_SPC_SMPTE170M:
				break;
			default:
				return false;
		}
		// SDR only: HDR requires explicit tone mapping, not silent clipping.
		if (source->color_trc == AVCOL_TRC_SMPTE2084 || source->color_trc == AVCOL_TRC_ARIB_STD_B67 ||
				source->color_primaries == AVCOL_PRI_BT2020) {
			return false;
		}
		float kg = 1.0f - kr - kb;
		params.coefficients[0] = 2.0f * (1.0f - kr);
		params.coefficients[1] = -2.0f * kb * (1.0f - kb) / kg;
		params.coefficients[2] = -2.0f * kr * (1.0f - kr) / kg;
		params.coefficients[3] = 2.0f * (1.0f - kb);
		float maximum = ten_bit ? 65535.0f : 255.0f;
		float unit = ten_bit ? 256.0f : 1.0f;
		bool full_range = source->color_range == AVCOL_RANGE_JPEG;
		params.range[0] = full_range ? 0.0f : 16.0f * unit / maximum;
		params.range[1] = 128.0f * unit / maximum;
		params.range[2] = maximum / (full_range ? (ten_bit ? 65472.0f : 255.0f) : 219.0f * unit);
		params.range[3] = maximum / (full_range ? (ten_bit ? 65472.0f : 255.0f) : 224.0f * unit);
		params.size[0] = source->width;
		params.size[1] = source->height;
		if (state->logged_format != vk_frames->format[0]) {
			print_verbose(ten_bit ? "FFmpeg Vulkan conversion input: P010 (10-bit SDR)." : "FFmpeg Vulkan conversion input: NV12 (8-bit SDR).");
			state->logged_format = vk_frames->format[0];
		}
		return true;
	}

	static void record(RenderingDeviceDriver *p_driver, RenderingDeviceDriver::CommandBufferID p_buffer, void *p_userdata) {
		ConversionJob *job = static_cast<ConversionJob *>(p_userdata);
		RenderingDeviceDriverVulkan *driver = static_cast<RenderingDeviceDriverVulkan *>(p_driver);
		AVHWFramesContext *frames = reinterpret_cast<AVHWFramesContext *>(job->frame->get_frame()->hw_frames_ctx->data);
		AVVulkanFramesContext *vk_frames = static_cast<AVVulkanFramesContext *>(frames->hwctx);
		AVVkFrame *gpu = reinterpret_cast<AVVkFrame *>(job->frame->get_frame()->data[0]);
		vk_frames->lock_frame(frames, gpu);
		uint64_t next_value = gpu->sem_value[0] + 1;
		if (!driver->command_buffer_add_timeline_dependency(p_buffer, gpu->sem[0], gpu->sem_value[0], next_value)) {
			vk_frames->unlock_frame(frames, gpu);
			job->failed->set();
			ERR_PRINT("FFmpeg Vulkan conversion could not attach its submission dependency.");
			return;
		}
		VkCommandBuffer command = driver->command_buffer_get_vulkan_handle(p_buffer);
		VkImageMemoryBarrier2 barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		barrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
		barrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
		barrier.oldLayout = gpu->layout[0];
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = gpu->img[0];
		barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		VkDependencyInfo dependency = {};
		dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		dependency.imageMemoryBarrierCount = 1;
		dependency.pImageMemoryBarriers = &barrier;
		vkCmdPipelineBarrier2(command, &dependency);
		vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_COMPUTE, job->state->pipeline);
		vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_COMPUTE, job->state->layout, 0, 1, &job->set, 0, nullptr);
		vkCmdPushConstants(command, job->state->layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(job->params), &job->params);
		vkCmdDispatch(command, (job->params.size[0] + 7) / 8, (job->params.size[1] + 7) / 8, 1);
#ifdef FFMPEG_GPU_TESTS_ENABLED
		if (job->validation_result) {
			VkImageMemoryBarrier2 ready = {};
			ready.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
			ready.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
			ready.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
			ready.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
			ready.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
			ready.oldLayout = ready.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			ready.srcQueueFamilyIndex = ready.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			ready.image = job->output_image;
			ready.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
			VkDependencyInfo check = {};
			check.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
			check.imageMemoryBarrierCount = 1;
			check.pImageMemoryBarriers = &ready;
			vkCmdPipelineBarrier2(command, &check);
			vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_COMPUTE, job->state->validation_pipeline);
			vkCmdDispatch(command, 1, 1, 1);
			VkBufferMemoryBarrier2 host = {};
			host.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
			host.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
			host.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
			host.dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT;
			host.dstAccessMask = VK_ACCESS_2_HOST_READ_BIT;
			host.srcQueueFamilyIndex = host.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			host.buffer = job->validation_buffer;
			host.size = VK_WHOLE_SIZE;
			check.imageMemoryBarrierCount = 0;
			check.bufferMemoryBarrierCount = 1;
			check.pBufferMemoryBarriers = &host;
			vkCmdPipelineBarrier2(command, &check);
		}
#endif
		// Restore FFmpeg's layout and signal a new timeline value only after
		// this exact command buffer has finished reading the image.
		barrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
		barrier.srcAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
		barrier.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
		barrier.dstAccessMask = 0;
		barrier.newLayout = barrier.oldLayout;
		barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		vkCmdPipelineBarrier2(command, &dependency);
		gpu->sem_value[0] = next_value;
		gpu->access[0] = VK_ACCESS_2_NONE; // Released; the next FFmpeg use acquires after this timeline value.
		vk_frames->unlock_frame(frames, gpu);
		job->recorded = true;
#ifdef FFMPEG_GPU_TESTS_ENABLED
		submitted_jobs.increment();
		{
			MutexLock lock(statistics_mutex);
			TextureStatistics *statistics = texture_statistics.getptr(job->output_rid);
			if (statistics) {
				statistics->submitted++;
				AVFrame *source = job->frame->get_frame();
				statistics->last_pts = source->best_effort_timestamp != AV_NOPTS_VALUE ? source->best_effort_timestamp : source->pts;
			}
		}
#endif
	}

	~ConversionJob() {
#ifdef FFMPEG_GPU_TESTS_ENABLED
		if (recorded) {
			completed_jobs.increment();
			uint32_t checked = 0;
			uint32_t failures = 0;
			if (validation_result) {
				const volatile uint32_t *result = static_cast<volatile uint32_t *>(validation_result);
				checked = result[0];
				failures = result[1] + (checked != 8 ? 1 : 0);
				color_checked.add(checked);
				color_failed.add(failures);
			}
			MutexLock lock(statistics_mutex);
			TextureStatistics *statistics = texture_statistics.getptr(output_rid);
			if (statistics) {
				statistics->completed++;
				statistics->color_checked += checked;
				statistics->color_failed += failures;
			}
		}
		if (validation_result) {
			vkUnmapMemory(state->device, validation_memory);
		}
		if (validation_buffer) {
			vkDestroyBuffer(state->device, validation_buffer, nullptr);
		}
		if (validation_memory) {
			vkFreeMemory(state->device, validation_memory, nullptr);
		}
#endif
		if (pool) {
			vkDestroyDescriptorPool(state->device, pool, nullptr);
		}
		for (VkImageView view : planes) {
			if (view) {
				vkDestroyImageView(state->device, view, nullptr);
			}
		}
	}
};
} // namespace

#ifdef FFMPEG_GPU_TESTS_ENABLED
void VulkanVideoConverter::enable_color_validation() {
	validate_colors.set();
}

Dictionary VulkanVideoConverter::get_statistics(RID p_texture) {
	Dictionary result;
	if (p_texture.is_valid()) {
		MutexLock lock(statistics_mutex);
		const TextureStatistics *statistics = texture_statistics.getptr(p_texture);
		if (statistics) {
			result["submitted"] = statistics->submitted;
			result["completed"] = statistics->completed;
			result["last_pts"] = statistics->last_pts;
			result["color_checked"] = statistics->color_checked;
			result["color_failed"] = statistics->color_failed;
		}
		return result;
	}
	result["submitted"] = submitted_jobs.get();
	result["completed"] = completed_jobs.get();
	result["color_checked"] = color_checked.get();
	result["color_failed"] = color_failed.get();
	result["dpb_gate_ok"] = supports_output_usage(VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR | VK_IMAGE_USAGE_SAMPLED_BIT) &&
			!supports_output_usage(VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR | VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR | VK_IMAGE_USAGE_SAMPLED_BIT);
	return result;
}
#endif

VulkanVideoConverter::VulkanVideoConverter() {
	output.instantiate();
}

VulkanVideoConverter::~VulkanVideoConverter() {
	RenderingServer::get_singleton()->call_on_render_thread(
			callable_mp_static(&VulkanVideoConverter::_free_render_thread).bind(state, output));
}

void VulkanVideoConverter::_free_render_thread(Ref<RefCounted> p_state, Ref<Texture2DRD> p_output) {
	RID texture = p_output->get_texture_rd_rid();
	p_output->set_texture_rd_rid(RID());
	if (texture.is_valid()) {
		RenderingServer::get_singleton()->get_rendering_device()->free_rid(texture);
#ifdef FFMPEG_GPU_TESTS_ENABLED
		MutexLock lock(statistics_mutex);
		texture_statistics.erase(texture);
#endif
	}
}

void VulkanVideoConverter::convert(const Ref<FFmpegFrame> &p_frame) {
	RenderingServer::get_singleton()->call_on_render_thread(
			callable_mp_static(&VulkanVideoConverter::_convert_render_thread).bind(Ref<RefCounted>(this), p_frame));
}

void VulkanVideoConverter::clear() {
	RenderingServer::get_singleton()->call_on_render_thread(
			callable_mp_static(&VulkanVideoConverter::_clear_render_thread).bind(Ref<RefCounted>(this)));
}

void VulkanVideoConverter::_clear_render_thread(Ref<RefCounted> p_self) {
	VulkanVideoConverter *converter = static_cast<VulkanVideoConverter *>(p_self.ptr());
	RID texture = converter->output->get_texture_rd_rid();
	if (texture.is_valid()) {
		RenderingServer::get_singleton()->get_rendering_device()->texture_clear(texture, Color(0, 0, 0, 0), 0, 1, 0, 1);
	}
}

void VulkanVideoConverter::_convert_render_thread(Ref<RefCounted> p_self, Ref<FFmpegFrame> p_frame) {
	VulkanVideoConverter *converter = static_cast<VulkanVideoConverter *>(p_self.ptr());
	if (converter->has_failed()) {
		return;
	}
	RD *rd = RenderingServer::get_singleton()->get_rendering_device();
	Ref<ConversionState> state(static_cast<ConversionState *>(converter->state.ptr()));
	if (state.is_null()) {
		state.instantiate();
		if (!state->initialize(rd)) {
			converter->failed.set();
			ERR_PRINT("FFmpeg Vulkan conversion pipeline initialization failed.");
			return;
		}
		converter->state = state;
	}
	AVFrame *frame = p_frame->get_frame();
	if (frame->width <= 0 || frame->height <= 0) {
		converter->failed.set();
		ERR_PRINT("FFmpeg Vulkan conversion rejected invalid frame dimensions.");
		return;
	}
	RID texture = converter->output->get_texture_rd_rid();
	if (texture.is_null() || converter->output->get_width() != frame->width || converter->output->get_height() != frame->height) {
		converter->output->set_texture_rd_rid(RID());
		if (texture.is_valid()) {
			rd->free_rid(texture);
#ifdef FFMPEG_GPU_TESTS_ENABLED
			MutexLock lock(statistics_mutex);
			texture_statistics.erase(texture);
#endif
		}
		RD::TextureFormat format;
		format.width = frame->width;
		format.height = frame->height;
		format.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
		format.shareable_formats.push_back(RD::DATA_FORMAT_R8G8B8A8_UNORM);
		format.shareable_formats.push_back(RD::DATA_FORMAT_R8G8B8A8_SRGB);
		format.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_STORAGE_BIT | RD::TEXTURE_USAGE_CAN_COPY_TO_BIT;
		texture = rd->texture_create(format, RD::TextureView());
		if (texture.is_null()) {
			converter->failed.set();
			return;
		}
#ifdef FFMPEG_GPU_TESTS_ENABLED
		{
			MutexLock lock(statistics_mutex);
			texture_statistics.insert(texture, TextureStatistics());
		}
#endif
		rd->set_resource_name(texture, "FFmpeg Vulkan RGBA output");
		print_verbose(vformat("FFmpeg Vulkan RGBA output image: 0x%x.", rd->get_driver_resource(RD::DRIVER_RESOURCE_TEXTURE, texture)));
		rd->texture_clear(texture, Color(0, 0, 0, 0), 0, 1, 0, 1);
		converter->output->set_texture_rd_rid(texture);
	}
	Ref<ConversionJob> job;
	job.instantiate();
	job->owner = p_self;
	job->state = state;
	job->frame = p_frame;
	job->failed = &converter->failed;
	job->output_rid = texture;
#ifdef FFMPEG_GPU_TESTS_ENABLED
	job->output_image = reinterpret_cast<VkImage>(rd->get_driver_resource(RD::DRIVER_RESOURCE_TEXTURE, texture));
#endif
	VkImageView view = reinterpret_cast<VkImageView>(rd->get_driver_resource(RD::DRIVER_RESOURCE_TEXTURE_VIEW, texture));
	if (!job->initialize(view)) {
		converter->failed.set();
		ERR_PRINT("FFmpeg Vulkan conversion rejected the frame format, color metadata, or image ownership; no CPU fallback is allowed.");
		return;
	}
	RD::CallbackResource resource;
	resource.rid = texture;
	resource.usage = RD::CALLBACK_RESOURCE_USAGE_STORAGE_IMAGE_READ_WRITE;
	if (rd->driver_callback_add(&ConversionJob::record, job.ptr(), VectorView<RD::CallbackResource>(&resource, 1)) != OK) {
		converter->failed.set();
		return;
	}
	rd->retain_until_frame_complete(job);
}
#endif
