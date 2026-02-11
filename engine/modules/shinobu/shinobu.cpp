#include "shinobu.h"
#include "shinobu_sound_source.h"

#include <memory>

#define MA_NO_VORBIS /* Disable the built-in Vorbis decoder to ensure the libvorbis decoder is picked. */
#define MA_NO_OPUS /* Disable the (not yet implemented) built-in Opus decoder to ensure the libopus decoder is picked. */
#define MINIAUDIO_IMPLEMENTATION
#define MA_DEBUG_OUTPUT

/* clang-format off */
#include "miniaudio/miniaudio.h"
#include "shinobu_channel_remap.h"
#include "shinobu_pitch_shift.h"
#include "shinobu_spectrum_analyzer.h"
/* clang-format on */

#include "core/os/os.h"
#include "miniaudio/extras/miniaudio_libvorbis.h"
#include "shinobu_macros.h"

Shinobu *Shinobu::singleton = nullptr;
SafeNumeric<uint64_t> Shinobu::sound_source_uid;

void Shinobu::_bind_methods() {
	ClassDB::bind_method(D_METHOD("create_group", "group_name", "parent_group"), &Shinobu::create_group);
	ClassDB::bind_method(D_METHOD("initialize"), &Shinobu::godot_initialize);
	ClassDB::bind_method(D_METHOD("get_initialization_error"), &Shinobu::get_initialization_error);
	ClassDB::bind_method(D_METHOD("register_sound_from_memory", "name_hint", "data"), &Shinobu::register_sound_from_memory);
	ClassDB::bind_method(D_METHOD("instantiate_spectrum_analyzer_effect"), &Shinobu::instantiate_spectrum_analyzer_effect);
	ClassDB::bind_method(D_METHOD("instantiate_pitch_shift"), &Shinobu::instantiate_pitch_shift);
	ClassDB::bind_method(D_METHOD("instantiate_channel_remap", "channel_count_in", "channel_count_out"), &Shinobu::instantiate_channel_remap);
	ClassDB::bind_method(D_METHOD("set_desired_buffer_size_msec", "desired_buffer_size"), &Shinobu::set_desired_buffer_size_msec);
	ClassDB::bind_method(D_METHOD("get_desired_buffer_size_msec"), &Shinobu::get_desired_buffer_size_msec);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "desired_buffer_size_msec"), "set_desired_buffer_size_msec", "get_desired_buffer_size_msec");
	ClassDB::bind_method(D_METHOD("set_master_volume", "linear_volume"), &Shinobu::set_master_volume);
	ClassDB::bind_method(D_METHOD("get_master_volume"), &Shinobu::get_master_volume);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "master_volume"), "set_master_volume", "get_master_volume");
	ClassDB::bind_method(D_METHOD("set_dsp_time", "new_time"), &Shinobu::set_dsp_time);
	ClassDB::bind_method(D_METHOD("get_dsp_time"), &Shinobu::get_dsp_time);
	ClassDB::bind_method(D_METHOD("get_actual_buffer_size"), &Shinobu::get_actual_buffer_size);
	ClassDB::bind_method(D_METHOD("get_current_backend_name"), &Shinobu::get_current_backend_name);
}

String Shinobu::get_initialization_error() const {
	return "";
}

static ma_result ma_decoding_backend_init__libvorbis(void *pUserData, ma_read_proc onRead, ma_seek_proc onSeek, ma_tell_proc onTell, void *pReadSeekTellUserData, const ma_decoding_backend_config *pConfig, const ma_allocation_callbacks *pAllocationCallbacks, ma_data_source **ppBackend) {
	ma_result result;
	ma_libvorbis *pVorbis;

	(void)pUserData;

	pVorbis = (ma_libvorbis *)ma_malloc(sizeof(*pVorbis), pAllocationCallbacks);
	if (pVorbis == NULL) {
		return MA_OUT_OF_MEMORY;
	}

	result = ma_libvorbis_init(onRead, onSeek, onTell, pReadSeekTellUserData, pConfig, pAllocationCallbacks, pVorbis);
	if (result != MA_SUCCESS) {
		ma_free(pVorbis, pAllocationCallbacks);
		return result;
	}

	*ppBackend = pVorbis;

	return MA_SUCCESS;
}

void *ma_malloc_godot(size_t p_size, void *p_user_data) {
	return Memory::alloc_static(p_size);
}

void *ma_realloc_godot(void *p, size_t p_size, void *p_user_data) {
	return Memory::realloc_static(p, p_size);
}

void ma_free_godot(void *p, void *p_user_data) {
	Memory::free_static(p);
}

static ma_result ma_decoding_backend_init_file__libvorbis(void *pUserData, const char *pFilePath, const ma_decoding_backend_config *pConfig, const ma_allocation_callbacks *pAllocationCallbacks, ma_data_source **ppBackend) {
	ma_result result;
	ma_libvorbis *pVorbis;

	(void)pUserData;

	pVorbis = (ma_libvorbis *)ma_malloc(sizeof(*pVorbis), pAllocationCallbacks);
	if (pVorbis == NULL) {
		return MA_OUT_OF_MEMORY;
	}

	result = ma_libvorbis_init_file(pFilePath, pConfig, pAllocationCallbacks, pVorbis);
	if (result != MA_SUCCESS) {
		ma_free(pVorbis, pAllocationCallbacks);
		return result;
	}

	*ppBackend = pVorbis;

	return MA_SUCCESS;
}

static void ma_decoding_backend_uninit__libvorbis(void *pUserData, ma_data_source *pBackend, const ma_allocation_callbacks *pAllocationCallbacks) {
	ma_libvorbis *pVorbis = (ma_libvorbis *)pBackend;

	(void)pUserData;

	ma_libvorbis_uninit(pVorbis, pAllocationCallbacks);
	ma_free(pVorbis, pAllocationCallbacks);
}

static ma_decoding_backend_vtable g_ma_decoding_backend_vtable_libvorbis = {
	ma_decoding_backend_init__libvorbis,
	ma_decoding_backend_init_file__libvorbis,
	NULL, /* onInitFileW() */
	NULL, /* onInitMemory() */
	ma_decoding_backend_uninit__libvorbis
};

Error Shinobu::godot_initialize() {
	return initialize(ma_backend_null);
}

ma_backend Shinobu::string_to_backend(String str) {
	str = str.to_lower();
	if (str == "wasapi") {
		return ma_backend_wasapi;
	} else if (str == "directsound") {
		return ma_backend_dsound;
	} else if (str == "winmm") {
		return ma_backend_winmm;
	} else if (str == "coreaudio") {
		return ma_backend_coreaudio;
	} else if (str == "sndio") {
		return ma_backend_sndio;
	} else if (str == "audio4") {
		return ma_backend_audio4;
	} else if (str == "oss") {
		return ma_backend_oss;
	} else if (str == "pulseaudio") {
		return ma_backend_pulseaudio;
	} else if (str == "alsa") {
		return ma_backend_alsa;
	} else if (str == "jack") {
		return ma_backend_jack;
	} else if (str == "aaudio") {
		return ma_backend_aaudio;
	} else if (str == "opensl") {
		return ma_backend_opensl;
	} else if (str == "webaudio") {
		return ma_backend_webaudio;
	}
	return ma_backend_null;
}

Error Shinobu::initialize(ma_backend forced_backend) {
	List<String> args = OS::get_singleton()->get_cmdline_args();
	ma_backend backend_to_force = ma_backend_null;

#ifdef X11_ENABLED
	// PipeWire on deck sucks, lets check if we are on a deck and force alsa by default
	String deck_variable = OS::get_singleton()->get_environment("SteamDeck");
	if (!deck_variable.is_empty() && deck_variable.to_int() > 0) {
		backend_to_force = ma_backend_alsa;
	}
#endif

	bool shinobu_clock_mix_size_compensation = true;

	List<String>::Element *I = args.front();
	while (I) {
		List<String>::Element *N = I->next();
		if (I->get() == "--shinobu-backend") {
			if (N) {
				backend_to_force = string_to_backend(N->get());
				N = N->next();
			}
		} else if (I->get() == "--shinobu-dev-disable-clock-msc") {
			shinobu_clock_mix_size_compensation = false;
		}
		I = N;
	}

	clock->set_use_mix_size_compensation(shinobu_clock_mix_size_compensation);

	clock->measure(0);

	ma_result result;

	ma_device_config device_config = ma_device_config_init(ma_device_type_playback);
	device_config.pUserData = this;
	device_config.dataCallback = ma_data_callback;
	device_config.playback.format = ma_format_f32;
	device_config.playback.channels = 2;
	device_config.performanceProfile = ma_performance_profile_low_latency;
	if (desired_buffer_size_msec > 0) {
		device_config.periodSizeInMilliseconds = desired_buffer_size_msec;
	}

	// This is necessary to enable WASAPI low latency mode
	device_config.wasapi.noAutoConvertSRC = true;

	ma_backend *backends = NULL;
	uint64_t backend_count = 0;

	if (backend_to_force != ma_backend_null) {
		backend_count = 1;
		backends = new ma_backend[1]{ backend_to_force };
	}

	ma_context_config ctx_config = ma_context_config_init();
	ctx_config.allocationCallbacks.onFree = &ma_free_godot;
	ctx_config.allocationCallbacks.onMalloc = &ma_malloc_godot;
	ctx_config.allocationCallbacks.onRealloc = &ma_realloc_godot;

	result = ma_context_init(backends, 1, &ctx_config, &context);
	MA_ERR_RET(result, "Context init failed");

	delete[] backends;

	result = ma_device_init(&context, &device_config, &device);
	MA_ERR_RET(result, "Device init failed");

	ma_engine_config engine_config = ma_engine_config_init();
	engine_config.pDevice = &device;
	engine_config.channels = 2;
	engine_config.pContext = &context;
	if (desired_buffer_size_msec > 0) {
		engine_config.periodSizeInMilliseconds = desired_buffer_size_msec;
	}

	// Setup libvorbis

	ma_resource_manager_config resourceManagerConfig;

	/*
	Custom backend vtables
	*/
	ma_decoding_backend_vtable *pCustomBackendVTables[] = {
		&g_ma_decoding_backend_vtable_libvorbis
	};

	/* Using custom decoding backends requires a resource manager. */
	resourceManagerConfig = ma_resource_manager_config_init();
	resourceManagerConfig.ppCustomDecodingBackendVTables = pCustomBackendVTables;
	resourceManagerConfig.customDecodingBackendCount = sizeof(pCustomBackendVTables) / sizeof(pCustomBackendVTables[0]);
	resourceManagerConfig.pCustomDecodingBackendUserData = NULL;
	resourceManagerConfig.decodedFormat = ma_format_f32;

	result = ma_resource_manager_init(&resourceManagerConfig, &resource_manager);

	MA_ERR_RET(result, "Resource manager init failed!");

	engine_config.pResourceManager = &resource_manager;

	result = ma_engine_init(&engine_config, &engine);

	MA_ERR_RET(result, "Audio engine init failed!");

	initialized = true;

	return OK;
}

void Shinobu::ma_data_callback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount) {
	Shinobu *shinobu = (Shinobu *)pDevice->pUserData;
	if (shinobu != NULL) {
		ma_engine_read_pcm_frames(&shinobu->engine, pOutput, frameCount, NULL);
		uint32_t sample_size_nsec = (frameCount * 1e+9) / ma_engine_get_sample_rate(&shinobu->engine);
		shinobu->clock->measure(sample_size_nsec);
	}
}

Shinobu::Shinobu() {
	clock.instantiate();
	singleton = this;
	sound_source_uid.set(0);
}

Ref<ShinobuClock> Shinobu::get_clock() {
	return clock;
}

ma_engine *Shinobu::get_engine() {
	return &engine;
}

Ref<ShinobuSoundSourceMemory> Shinobu::register_sound_from_memory(String m_name_hint, PackedByteArray m_data) {
	Ref<ShinobuSoundSourceMemory> source;
	source.instantiate(m_name_hint, m_data);
	return source;
}

Ref<ShinobuGroup> Shinobu::create_group(String m_group_name, Ref<ShinobuGroup> m_parent_group) {
	Ref<ShinobuGroup> out_group = memnew(ShinobuGroup(m_group_name, m_parent_group));
	groups.push_back(out_group);
	return out_group;
}

Ref<ShinobuSpectrumAnalyzerEffect> Shinobu::instantiate_spectrum_analyzer_effect() {
	return memnew(ShinobuSpectrumAnalyzerEffect(2));
}

Ref<ShinobuPitchShiftEffect> Shinobu::instantiate_pitch_shift() {
	return memnew(ShinobuPitchShiftEffect(2));
}

Ref<ShinobuChannelRemapEffect> Shinobu::instantiate_channel_remap(uint32_t channel_count_in, uint32_t channel_count_out) {
	return memnew(ShinobuChannelRemapEffect(channel_count_in, channel_count_out));
}

void Shinobu::set_desired_buffer_size_msec(uint64_t m_new_buffer_size) {
	desired_buffer_size_msec = m_new_buffer_size;
}

uint64_t Shinobu::get_desired_buffer_size_msec() const {
	return desired_buffer_size_msec;
}

Error Shinobu::set_master_volume(float m_linear_volume) {
	MA_ERR_RET(ma_engine_set_volume(&engine, m_linear_volume), "Error setting volume");
	return OK;
}

float Shinobu::get_master_volume() {
	ma_node *endpoint = ma_engine_get_endpoint(&engine);
	return ma_node_get_output_bus_volume(endpoint, 0);
}

uint64_t Shinobu::get_dsp_time() const {
	return ma_engine_get_time(&engine) / (float)(ma_engine_get_sample_rate(&engine) / 1000.0f);
}

Error Shinobu::set_dsp_time(uint64_t m_new_time_msec) {
	ma_result result = ma_engine_set_time(&engine, m_new_time_msec * (float)(ma_engine_get_sample_rate(&engine) / 1000.0f));
	MA_ERR_RET(result, "Error setting DSP time");
	return OK;
}

String Shinobu::get_current_backend_name() const {
	return ma_get_backend_name(context.backend);
}

uint64_t Shinobu::get_actual_buffer_size() const {
	return device.playback.internalPeriodSizeInFrames / (double)(device.playback.internalSampleRate / 1000.0);
}

Shinobu::~Shinobu() {
	if (initialized) {
		ma_engine_uninit(&engine);
		ma_resource_manager_uninit(&resource_manager);
		ma_device_uninit(&device);
		ma_context_uninit(&context);
	}
}