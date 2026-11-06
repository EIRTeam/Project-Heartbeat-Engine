#include "shinobu_sound_source.h"
#include "core/os/os.h"
#include "shinobu.h"
#include "shinobu_macros.h"
#include "thirdparty/ebur128/ebur128.h"

void ShinobuSoundSource::_bind_methods() {
	ClassDB::bind_method(D_METHOD("instantiate", "group", "use_source_channel_count"), &ShinobuSoundSource::instantiate, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("get_channel_count"), &ShinobuSoundSource::get_channel_count);
	ClassDB::bind_method(D_METHOD("ebur128_get_loudness"), &ShinobuSoundSource::ebur128_get_loudness);
}

ShinobuSoundSource::ShinobuSoundSource(String m_name) {
	name = m_name;
}

float ShinobuSoundSource::ebur128_get_loudness() {
	ERR_FAIL_COND_V(!Shinobu::get_singleton() || !Shinobu::get_singleton()->is_initialized(), 0.0f);
	uint64_t start = OS::get_singleton()->get_ticks_usec();
	ma_sound sound;
	ma_format format;
	uint32_t channel_count;
	uint32_t sample_rate;
	ERR_FAIL_COND_V(instantiate_sound(Ref<ShinobuGroup>(), true, &sound) != OK, 0.0f);
	ma_sound_get_data_format(&sound, &format, &channel_count, &sample_rate, nullptr, 0);
	ebur128_state *state = ebur128_init(channel_count, sample_rate, EBUR128_MODE_I);
	if (state == nullptr) {
		ma_sound_uninit(&sound);
		ERR_FAIL_V(0.0f);
	}

	const uint64_t FRAMES_PER_CHUNK = 300000;
	Vector<float> chunk_data;
	chunk_data.resize(FRAMES_PER_CHUNK * channel_count);
	float *chunk_data_ptr = chunk_data.ptrw();
	ma_data_source *data_source = ma_sound_get_data_source(&sound);
	int frame_count = 0;
	uint64_t frames_read;
	while (ma_data_source_read_pcm_frames(data_source, chunk_data_ptr, (ma_uint64)FRAMES_PER_CHUNK, (ma_uint64 *)&frames_read) != MA_AT_END) {
		ebur128_add_frames_float(state, chunk_data_ptr, frames_read);
		frame_count += frames_read;
	}

	double loudness_global;
	ebur128_loudness_global(state, &loudness_global);
	ebur128_destroy(&state);
	ma_sound_uninit(&sound);
	print_line("Normalization done, took", (OS::get_singleton()->get_ticks_usec() - start) * 0.001, "milliseconds");
	return loudness_global;
}

uint32_t ShinobuSoundSource::get_channel_count() const {
	ERR_FAIL_COND_V(!Shinobu::get_singleton() || !Shinobu::get_singleton()->is_initialized(), 0);
	uint32_t channel_count;
	// data sources cannot be reused, so this is the best we can do
	ma_resource_manager_data_source source = {};
	ma_result init_result = ma_resource_manager_data_source_init(ma_engine_get_resource_manager(Shinobu::get_singleton()->get_engine()), name.utf8().get_data(), 0, nullptr, &source);
	ERR_FAIL_COND_V(init_result != MA_SUCCESS, 0);
	ma_result format_result = ma_resource_manager_data_source_get_data_format(&source, nullptr, &channel_count, nullptr, nullptr, 0);
	ma_resource_manager_data_source_uninit(&source);
	ERR_FAIL_COND_V(format_result != MA_SUCCESS, 0);
	return channel_count;
}

const String ShinobuSoundSource::get_name() const {
	return name;
}

ma_result ShinobuSoundSource::get_result() const {
	return result;
}

uint64_t ShinobuSoundSource::get_fixed_length() const {
	return 0.0f;
}

ShinobuSoundPlayer *ShinobuSoundSource::instantiate(Ref<ShinobuGroup> m_group, bool m_use_source_channel_count) {
	ERR_FAIL_COND_V(!Shinobu::get_singleton() || !Shinobu::get_singleton()->is_initialized(), nullptr);
	ERR_FAIL_COND_V(!m_use_source_channel_count && m_group.is_null(), nullptr);
	return memnew(ShinobuSoundPlayer(this, m_group, m_use_source_channel_count));
}

ShinobuSoundSource::~ShinobuSoundSource() {}

Error ShinobuSoundSourceMemory::instantiate_sound(Ref<ShinobuGroup> m_group, bool use_source_channel_count, ma_sound *p_sound) {
	ERR_FAIL_NULL_V(p_sound, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(!Shinobu::get_singleton() || !Shinobu::get_singleton()->is_initialized(), ERR_UNCONFIGURED);
	ERR_FAIL_COND_V(!use_source_channel_count && (m_group.is_null() || m_group->get_group() == nullptr), ERR_INVALID_PARAMETER);
	ma_sound_config config = ma_sound_config_init();
	CharString string_data = name.utf8();
	config.pFilePath = string_data.ptr();
	config.flags = config.flags | MA_SOUND_FLAG_NO_SPATIALIZATION;
	if (use_source_channel_count) {
		config.flags = config.flags | MA_SOUND_FLAG_NO_DEFAULT_ATTACHMENT;
		config.channelsOut = MA_SOUND_SOURCE_CHANNEL_COUNT;
	} else {
		config.pInitialAttachment = m_group->get_group();
	}

	ma_engine *engine = Shinobu::get_singleton()->get_engine();

	MA_ERR_RET(ma_sound_init_ex(engine, &config, p_sound), "Error initializing sound");
	return OK;
}

ShinobuSoundSourceMemory::ShinobuSoundSourceMemory(String m_name, PackedByteArray m_in_data) :
		ShinobuSoundSource(m_name) {
	data = m_in_data;
	ma_engine *engine = Shinobu::get_singleton()->get_engine();
	name = vformat("%s_%d", name, Shinobu::get_singleton()->get_inc_sound_source_uid());
	result = ma_resource_manager_register_encoded_data(ma_engine_get_resource_manager(engine), name.utf8().get_data(), (void *)data.ptr(), data.size());
	MA_ERR(result, "Error registering sound data");
}

ShinobuSoundSourceMemory::~ShinobuSoundSourceMemory() {
	if (result != MA_SUCCESS || !Shinobu::get_singleton() || !Shinobu::get_singleton()->is_initialized()) {
		return;
	}
	ma_engine *engine = Shinobu::get_singleton()->get_engine();
	ma_resource_manager_unregister_data(ma_engine_get_resource_manager(engine), name.utf8().get_data());
}
