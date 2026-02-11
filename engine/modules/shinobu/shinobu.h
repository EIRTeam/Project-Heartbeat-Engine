#ifndef SHINOBU_H
#define SHINOBU_H

#include <memory>
#include <vector>

#include "core/object/object.h"
#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "shinobu_clock.h"
#include "shinobu_group.h"
#include "shinobu_sound_source.h"
#include "miniaudio/miniaudio.h"

class Shinobu : public Object {
	GDCLASS(Shinobu, Object);

private:
	// This is a hack, because miniaudio resource manager sounds require a unique name
	// however, since we don't have those and we are leaving ensuring uniqueness to the underlying
	// program we might have the same sound source name loaded in memory twice
	static SafeNumeric<uint64_t> sound_source_uid;

	Ref<ShinobuClock> clock;
	static Shinobu *singleton;
	static ma_backend string_to_backend(String str);
	ma_engine engine;
	ma_device device;
	ma_resource_manager resource_manager;
	ma_context context;
	String error_message;
	uint64_t desired_buffer_size_msec = 10;

	Vector<Ref<ShinobuGroup>> groups;

	static void ma_data_callback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount);

	float master_volume = 1.0f;
	bool initialized = false;

protected:
	static void _bind_methods();

public:
	_FORCE_INLINE_ static Shinobu *get_singleton() { return singleton; }

	String get_initialization_error() const;

	Ref<ShinobuClock> get_clock();
	ma_engine *get_engine();
	Error initialize(ma_backend forced_backend);
	Error godot_initialize();
	_FORCE_INLINE_ static uint64_t get_inc_sound_source_uid() { return sound_source_uid.postincrement(); };

	Ref<ShinobuSoundSourceMemory> register_sound_from_memory(String m_name_hint, PackedByteArray m_data);
	Ref<ShinobuGroup> create_group(String m_group_name, Ref<ShinobuGroup> m_parent_group = nullptr);

	Ref<ShinobuSpectrumAnalyzerEffect> instantiate_spectrum_analyzer_effect();
	Ref<ShinobuPitchShiftEffect> instantiate_pitch_shift();
	Ref<ShinobuChannelRemapEffect> instantiate_channel_remap(uint32_t channel_count_in, uint32_t channel_count_out);

	void set_desired_buffer_size_msec(uint64_t m_new_buffer_size);
	uint64_t get_desired_buffer_size_msec() const;

	Error set_master_volume(float m_linear_volume);
	float get_master_volume();

	uint64_t get_dsp_time() const;
	Error set_dsp_time(uint64_t m_new_time_msec);

	uint64_t get_actual_buffer_size() const;
	String get_current_backend_name() const;

	Shinobu();
	~Shinobu();
};
#endif