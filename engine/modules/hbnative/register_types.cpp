/* register_types.cpp */

#include "register_types.h"

#include "core/config/engine.h"
#include "core/object/class_db.h"
#include "diva/bone_db.h"
#include "diva/diva_object.h"
#include "interval_tree.h"
#include "modules/hbnative/ph_blur_controls.h"
#include "multi_spin_box.h"
#include "ph_audio_stream_editor.h"
#include "ph_audio_stream_preview.h"
#include "ph_blur_controls.h"
#include "ph_singleton.h"

#include "process/process.h"
#include "process/process_tiny_process_lib.h"
#ifdef UNIX_ENABLED
#include "process/process_unix.h"
#endif
#ifdef WINDOWS_ENABLED
#include "process/process_windows.h"
#endif

#include "rectpack/rectpack.h"
#include "threen.h"

static PHAudioStreamPreviewGenerator *preview_generator_ptr = NULL;
static PHNative *ph_ptr = NULL;

const char *blur_shader_code =
		"shader_type canvas_item;"
		"uniform sampler2D SCREEN_TEXTURE : hint_screen_texture, filter_linear_mipmap;"
		"uniform float blur_amount = 2.5;"
		"void fragment() {"
		"	COLOR = texture(SCREEN_TEXTURE, SCREEN_UV, blur_amount);"
		"	COLOR.a = 1.0;"
		"}";

void initialize_hbnative_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	preview_generator_ptr = memnew(PHAudioStreamPreviewGenerator);
	ph_ptr = memnew(PHNative);

#ifdef UNIX_ENABLED
	ProcessUnix::make_default();
#endif

#ifdef WINDOWS_ENABLED
	ProcessWindows::make_default();
#endif

	GDREGISTER_CLASS(PHAudioStreamPreviewGenerator);
	GDREGISTER_CLASS(PHAudioStreamEditor);
	GDREGISTER_CLASS(PHAudioStreamPreview);

	GDREGISTER_CLASS(HBButtonBlurEX);
	GDREGISTER_CLASS(HBPanelContainerBlurEX);
	GDREGISTER_CLASS(HBPanelBlurEX);

	Ref<Shader> blur_shader;
	if (RenderingServer::get_singleton()) {
		blur_shader.instantiate();
		blur_shader->set_code(blur_shader_code);

		Ref<ShaderMaterial> blur_shader_mat;
		blur_shader_mat.instantiate();
		blur_shader_mat->set_shader(blur_shader);
		HBStyleboxBlurDrawer::blur_material = blur_shader_mat;
	}

	GDREGISTER_CLASS(Threen);
	GDREGISTER_ABSTRACT_CLASS(Process);
	GDREGISTER_ABSTRACT_CLASS(PHNative);
	GDREGISTER_CLASS(MultiSpinBox);
	GDREGISTER_CLASS(HBIntervalTree);
	GDREGISTER_CLASS(DIVABoneDB);
	GDREGISTER_CLASS(DIVASkeleton);
	GDREGISTER_CLASS(DIVAObjectSet);
	GDREGISTER_ABSTRACT_CLASS(HBRectPack);
	Engine::get_singleton()->add_singleton(Engine::Singleton("PHAudioStreamPreviewGenerator", PHAudioStreamPreviewGenerator::get_singleton()));
	Engine::get_singleton()->add_singleton(Engine::Singleton("PHNative", PHNative::get_singleton()));
}

void uninitialize_hbnative_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	memdelete(preview_generator_ptr);
	memdelete(ph_ptr);
	HBStyleboxBlurDrawer::blur_material.unref();
}
