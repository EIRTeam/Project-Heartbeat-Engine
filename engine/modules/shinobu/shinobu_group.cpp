#include "shinobu_group.h"
#include "shinobu.h"
#include "shinobu_effects.h"
#include "shinobu_macros.h"

void ShinobuGroup::_bind_methods() {
	ClassDB::bind_method(D_METHOD("connect_to_effect", "effect"), &ShinobuGroup::connect_to_effect);
	ClassDB::bind_method(D_METHOD("connect_to_endpoint"), &ShinobuGroup::connect_to_endpoint);
	ClassDB::bind_method(D_METHOD("set_volume", "linear_volume"), &ShinobuGroup::set_volume);
	ClassDB::bind_method(D_METHOD("get_volume"), &ShinobuGroup::get_volume);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "volume"), "set_volume", "get_volume");
}

ma_sound_group *ShinobuGroup::get_group() {
	return &group;
}

void ShinobuGroup::set_volume(float m_linear_volume) {
	ma_sound_group_set_volume(&group, m_linear_volume);
}

float ShinobuGroup::get_volume() const {
	return ma_sound_group_get_volume(&group);
}

Error ShinobuGroup::connect_to_endpoint() {
	ma_engine *engine = Shinobu::get_singleton()->get_engine();
	MA_ERR_RET(ma_node_attach_output_bus(&group, 0, ma_engine_get_endpoint(engine), 0), "Error connecting group to endpoint");
	return OK;
}

Error ShinobuGroup::connect_to_effect(Ref<ShinobuEffect> m_effect) {
	MA_ERR_RET(ma_node_attach_output_bus(&group, 0, m_effect->get_node(), 0), "Error connecting group to effect");
	return OK;
}

ShinobuGroup::ShinobuGroup(String m_group_name, Ref<ShinobuGroup> m_parent_group) {
	ma_engine *engine = Shinobu::get_singleton()->get_engine();

	ma_sound_group *parent_group = nullptr;

	if (!m_parent_group.is_null() && m_parent_group.is_valid()) {
		parent_group = m_parent_group->get_group();
	}

	MA_ERR(ma_sound_group_init(engine, 0, parent_group, &group), "Error creating group");
}

ShinobuGroup::~ShinobuGroup() {
	ma_sound_group_uninit(&group);
}