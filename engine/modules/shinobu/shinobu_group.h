#ifndef SHINOBU_GROUP_H
#define SHINOBU_GROUP_H

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "miniaudio/miniaudio.h"
#include <memory>

class ShinobuEffect;

class ShinobuGroup : public RefCounted {
	GDCLASS(ShinobuGroup, RefCounted);
	String name;
	String error_message;
	ma_sound_group group;

protected:
	static void _bind_methods();

public:
	ma_sound_group *get_group();

	void set_volume(float m_linear_volume);
	float get_volume() const;
	Error connect_to_endpoint();
	Error connect_to_effect(Ref<ShinobuEffect> m_effect);

	ShinobuGroup(String m_group_name, Ref<ShinobuGroup> m_parent_group);
	~ShinobuGroup();
};
#endif