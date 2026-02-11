#include "shinobu_sound_player.h"
#include "shinobu.h"
#include "shinobu_macros.h"

void ShinobuSoundPlayer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("start"), &ShinobuSoundPlayer::start);
	ClassDB::bind_method(D_METHOD("stop"), &ShinobuSoundPlayer::stop);
	ClassDB::bind_method(D_METHOD("set_pitch_scale", "pitch_scale"), &ShinobuSoundPlayer::set_pitch_scale);
	ClassDB::bind_method(D_METHOD("get_pitch_scale"), &ShinobuSoundPlayer::get_pitch_scale);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "pitch_scale"), "set_pitch_scale", "get_pitch_scale");
	ClassDB::bind_method(D_METHOD("schedule_start_time", "global_time_msec"), &ShinobuSoundPlayer::schedule_start_time);
	ClassDB::bind_method(D_METHOD("schedule_stop_time", "global_time_msec"), &ShinobuSoundPlayer::schedule_stop_time);
	ClassDB::bind_method(D_METHOD("get_playback_position_nsec"), &ShinobuSoundPlayer::get_playback_position_nsec);
	ClassDB::bind_method(D_METHOD("get_playback_position_msec"), &ShinobuSoundPlayer::get_playback_position_msec);
	ClassDB::bind_method(D_METHOD("is_at_stream_end"), &ShinobuSoundPlayer::is_at_stream_end);
	ClassDB::bind_method(D_METHOD("is_playing"), &ShinobuSoundPlayer::is_playing);
	ClassDB::bind_method(D_METHOD("set_volume", "linear_volume"), &ShinobuSoundPlayer::set_volume);
	ClassDB::bind_method(D_METHOD("get_volume"), &ShinobuSoundPlayer::get_volume);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "volume"), "set_volume", "get_volume");
	ClassDB::bind_method(D_METHOD("set_looping_enabled", "looping"), &ShinobuSoundPlayer::set_looping_enabled);
	ClassDB::bind_method(D_METHOD("is_looping_enabled"), &ShinobuSoundPlayer::is_looping_enabled);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "looping_enabled"), "set_looping_enabled", "is_looping_enabled");
	ClassDB::bind_method(D_METHOD("connect_sound_to_effect", "effect"), &ShinobuSoundPlayer::connect_sound_to_effect);
	ClassDB::bind_method(D_METHOD("connect_sound_to_group", "group"), &ShinobuSoundPlayer::connect_sound_to_group);
	ClassDB::bind_method(D_METHOD("get_channel_count"), &ShinobuSoundPlayer::get_channel_count);
	ClassDB::bind_method(D_METHOD("seek", "to_time_msec"), &ShinobuSoundPlayer::seek);
	ClassDB::bind_method(D_METHOD("get_length_msec"), &ShinobuSoundPlayer::get_length_msec);
	ClassDB::bind_method(D_METHOD("fade", "fade_duration", "volume_begin", "volume_end"), &ShinobuSoundPlayer::fade);
}

Error ShinobuSoundPlayer::start() {
	MA_ERR_RET(ma_sound_start(&sound), "Error starting sound");
	return OK;
}

Error ShinobuSoundPlayer::stop() {
	MA_ERR_RET(ma_sound_stop(&sound), "Error stopping sound");
	return OK;
}

void ShinobuSoundPlayer::set_pitch_scale(float m_pitch_scale) {
	ma_sound_set_pitch(&sound, m_pitch_scale);
}

float ShinobuSoundPlayer::get_pitch_scale() {
	return ma_sound_get_pitch(&sound);
}

void ShinobuSoundPlayer::schedule_start_time(uint64_t m_global_time_msec) {
	start_time_msec = m_global_time_msec;
	ma_sound_set_start_time_in_milliseconds(&sound, m_global_time_msec);
}

void ShinobuSoundPlayer::schedule_stop_time(uint64_t m_global_time_msec) {
	ma_sound_set_stop_time_in_milliseconds(&sound, m_global_time_msec);
}

int64_t ShinobuSoundPlayer::get_playback_position_nsec() {
	Ref<ShinobuClock> clock = Shinobu::get_singleton()->get_clock();
	ma_engine *engine = Shinobu::get_singleton()->get_engine();

	ma_uint64 pos_frames = 0;
	ma_result result = ma_sound_get_cursor_in_pcm_frames(&sound, &pos_frames);
	int64_t out_pos = 0;
	uint32_t sample_rate;
	if (result == MA_SUCCESS) {
		result = ma_sound_get_data_format(&sound, NULL, NULL, &sample_rate, NULL, 0);
		if (result == MA_SUCCESS) {
			out_pos = (pos_frames * 1e+9) / sample_rate;
		}
	}

	// This allows the return of negative playback time
	// seconds to nanoseconds = x * 1_000_000_000
	// milliseconds to nanoseconds = x * 1_000_000
	uint64_t dsp_time_nsec = (ma_engine_get_time(engine) * 1e+9) / ma_engine_get_sample_rate(engine);
	uint64_t start_time_nsec = start_time_msec * 1e+6;

	if (!is_playing() && start_time_nsec > dsp_time_nsec) {
		return dsp_time_nsec - start_time_nsec + out_pos;
	}

	if (is_playing()) {
		int64_t engine_offset = clock->get_current_offset_nsec();
		engine_offset = ma_sound_get_pitch(&sound) * engine_offset;
		out_pos += engine_offset;
	}

	return out_pos;
}

int64_t ShinobuSoundPlayer::get_playback_position_msec() {
	return get_playback_position_nsec() / 1e+6;
}

bool ShinobuSoundPlayer::is_at_stream_end() const {
	return (bool)ma_sound_at_end(&sound);
}

bool ShinobuSoundPlayer::is_playing() const {
	return (bool)ma_sound_is_playing(&sound);
}

void ShinobuSoundPlayer::set_volume(float m_linear_volume) {
	ma_sound_set_volume(&sound, m_linear_volume);
}

float ShinobuSoundPlayer::get_volume() const {
	return ma_sound_get_volume(&sound);
}

void ShinobuSoundPlayer::set_looping_enabled(bool m_looping) {
	ma_sound_set_looping(&sound, m_looping);
}

bool ShinobuSoundPlayer::is_looping_enabled() const {
	return (bool)ma_sound_is_looping(&sound);
}

Error ShinobuSoundPlayer::connect_sound_to_effect(Ref<ShinobuEffect> m_effect) {
	MA_ERR_RET(ma_node_attach_output_bus(&sound, 0, m_effect->get_node(), 0), "Error attaching sound to effect");
	return OK;
}

uint64_t ShinobuSoundPlayer::get_channel_count() {
	ma_uint32 channel_count;
	ma_sound_get_data_format(&sound, NULL, &channel_count, NULL, NULL, 0);
	return channel_count;
}

Error ShinobuSoundPlayer::connect_sound_to_group(Ref<ShinobuGroup> m_group) {
	MA_ERR_RET(ma_node_attach_output_bus(&sound, 0, m_group->get_group(), 0), "Error attaching sound to group");
	return OK;
}

void ShinobuSoundPlayer::fade(int p_duration_ms, float p_volume_begin, float p_volume_end) {
	ma_sound_set_fade_in_milliseconds(&sound, p_volume_begin, p_volume_end, p_duration_ms);
}

void ShinobuSoundPlayer::_notification(int p_notification) {
	switch (p_notification) {
		case NOTIFICATION_PAUSED: {
			if (!can_process()) {
				was_playing_before_pause = is_playing();
				ma_sound_stop(&sound);
			}
		} break;
		case NOTIFICATION_UNPAUSED: {
			if (was_playing_before_pause && !is_at_stream_end()) {
				ma_sound_start(&sound);
			}
		} break;
	}
}

Error ShinobuSoundPlayer::seek(int64_t to_time_msec) {
	// Sound MUST be stopped before seeking or we crash
	if (ma_sound_is_playing(&sound) == MA_TRUE) {
		ma_sound_stop(&sound);
	}
	uint32_t sample_rate;
	ma_sound_get_data_format(&sound, NULL, NULL, &sample_rate, NULL, 0);
	ma_result result = ma_sound_seek_to_pcm_frame(&sound, MAX(0.0f, to_time_msec * (float)(sample_rate / 1000.0f)));
	MA_ERR_RET(result, "Error seeking sound");
	return OK;
}

uint64_t ShinobuSoundPlayer::get_length_msec() {
	if (cached_length != -1) {
		return cached_length;
	}

	ma_uint64 p_length = 0;
	ma_sound_get_length_in_pcm_frames(&sound, &p_length);
	uint32_t sample_rate;
	ma_format format;
	ma_sound_get_data_format(&sound, &format, NULL, &sample_rate, NULL, 0);
	p_length /= (float)(sample_rate / 1000.0f);
	cached_length = p_length;

	return p_length;
}

ShinobuSoundPlayer::ShinobuSoundPlayer(Ref<ShinobuSoundSource> m_sound_source, Ref<ShinobuGroup> m_group, bool m_use_source_channel_count) {
	m_sound_source->instantiate_sound(m_group, m_use_source_channel_count, &sound);
	sound_source = m_sound_source;
}

ShinobuSoundPlayer::~ShinobuSoundPlayer() {
	ma_sound_uninit(&sound);
}