/* register_types.cpp */

#include "register_types.h"

#include "core/config/engine.h"
#include "shinobu.h"
#include "shinobu_effects.h"
#include "shinobu_sound_player.h"

static Shinobu *shinobu_ptr = NULL;

void initialize_shinobu_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	GDREGISTER_ABSTRACT_CLASS(ShinobuSoundPlayer);
	GDREGISTER_ABSTRACT_CLASS(ShinobuSoundSource);
	GDREGISTER_ABSTRACT_CLASS(ShinobuSoundSourceMemory);
	GDREGISTER_ABSTRACT_CLASS(ShinobuGroup);
	GDREGISTER_ABSTRACT_CLASS(ShinobuEffect);
	GDREGISTER_ABSTRACT_CLASS(ShinobuChannelRemapEffect);
	GDREGISTER_ABSTRACT_CLASS(ShinobuPitchShiftEffect);
	GDREGISTER_ABSTRACT_CLASS(ShinobuSpectrumAnalyzerEffect);
	GDREGISTER_CLASS(Shinobu);
	shinobu_ptr = memnew(Shinobu);
	Engine::get_singleton()->add_singleton(Engine::Singleton("Shinobu", Shinobu::get_singleton()));
}

void uninitialize_shinobu_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	memdelete(shinobu_ptr);
}
