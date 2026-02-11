#ifndef SHINOBU_EFFECTS_H
#define SHINOBU_EFFECTS_H

#include "core/object/ref_counted.h"

#include "shinobu_channel_remap.h"
#include "shinobu_pitch_shift.h"
#include "shinobu_spectrum_analyzer.h"

#include <memory>

class ShinobuGroup;

class ShinobuEffect : public RefCounted {
	GDCLASS(ShinobuEffect, RefCounted);

protected:
	String error_message = "";
	Error connect_to_node(ma_node *m_node);
	static void _bind_methods();

public:
	virtual ma_node *get_node() = 0;
	Error connect_to_effect(Ref<ShinobuEffect> m_effect);
	Error connect_to_group(Ref<ShinobuGroup> m_group);
	Error connect_to_endpoint();
};

class ShinobuChannelRemapEffect : public ShinobuEffect {
	GDCLASS(ShinobuChannelRemapEffect, ShinobuEffect);
	ma_channel_remap_node remap_node;

protected:
	static void _bind_methods();

public:
	ShinobuChannelRemapEffect(uint32_t in_channel_count, uint32_t out_channel_count);
	~ShinobuChannelRemapEffect();

	void set_weight(uint8_t channel_in, uint8_t channel_out, float weight);
	virtual ma_node *get_node() override;
};

class ShinobuPitchShiftEffect : public ShinobuEffect {
	GDCLASS(ShinobuPitchShiftEffect, ShinobuEffect);
	ma_pitch_shift_node pitch_shift_node;

protected:
	static void _bind_methods();

public:
	ShinobuPitchShiftEffect(uint32_t m_channel_count);
	~ShinobuPitchShiftEffect();

	void set_pitch_scale(float m_pitch_scale);
	float get_pitch_scale();

	virtual ma_node *get_node() override;
};

class ShinobuSpectrumAnalyzerEffect : public ShinobuEffect {
	GDCLASS(ShinobuSpectrumAnalyzerEffect, ShinobuEffect);
	ma_spectrum_analyzer_node analyzer_node;

protected:
	static void _bind_methods();

public:
	ShinobuSpectrumAnalyzerEffect(uint32_t m_channel_count);
	~ShinobuSpectrumAnalyzerEffect();

	Vector2 get_magnitude_for_frequency_range(float pBegin, float pEnd, ma_spectrum_magnitude_mode mode = MAGNITUDE_MAX);

	virtual ma_node *get_node() override;
};

VARIANT_ENUM_CAST(ma_spectrum_magnitude_mode);

#endif