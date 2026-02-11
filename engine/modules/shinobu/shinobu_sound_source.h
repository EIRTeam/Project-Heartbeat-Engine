
#ifndef SHINOBU_SOUND_SOURCE_H
#define SHINOBU_SOUND_SOURCE_H

#include "miniaudio/miniaudio.h"
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "core/string/ustring.h"
#include "shinobu_group.h"
#include "shinobu_sound_player.h"

class ShinobuSoundSource : public RefCounted {
	GDCLASS(ShinobuSoundSource, RefCounted);

protected:
	String error_message;
	String name;
	ma_result result;

	static void _bind_methods();

public:
	virtual const String get_name() const;

	virtual ma_result get_result() const;

	virtual uint64_t get_fixed_length() const;
	ShinobuSoundPlayer *instantiate(Ref<ShinobuGroup> m_group, bool m_use_source_channel_count = false);

	ShinobuSoundSource(String m_name);

	float ebur128_get_loudness();
	uint32_t get_channel_count() const;
	virtual Error instantiate_sound(Ref<ShinobuGroup> m_group, bool use_source_channel_count, ma_sound *p_sound) = 0;

	virtual ~ShinobuSoundSource();
};

class ShinobuSoundSourceMemory : public ShinobuSoundSource {
	GDCLASS(ShinobuSoundSourceMemory, ShinobuSoundSource);
	PackedByteArray data;

public:
	virtual Error instantiate_sound(Ref<ShinobuGroup> m_group, bool use_source_channel_count, ma_sound *p_sound) override;
	ShinobuSoundSourceMemory(String p_name, PackedByteArray p_in_data);
	~ShinobuSoundSourceMemory();
	friend class ShinobuSoundPlayer;
};

#endif // SHINOBU_SOUND_SOURCE_H