#include "item_table.h"
#include "kv_table.h"
#include "sprite_db.h"

DIVACharacter ModuleTable::get_character(const StringName &p_key) {
	static HashMap<StringName, DIVACharacter> character_names;
	if (unlikely(character_names.size() == 0)) {
		character_names.insert(SNAME("MIKU"), MIKU);
		character_names.insert(SNAME("RIN"), RIN);
		character_names.insert(SNAME("LEN"), LEN);
		character_names.insert(SNAME("LUKA"), LUKA);
		character_names.insert(SNAME("NERU"), NERU);
		character_names.insert(SNAME("HAKU"), HAKU);
		character_names.insert(SNAME("KAITO"), KAITO);
		character_names.insert(SNAME("MEIKO"), MEIKO);
		character_names.insert(SNAME("SAKINE"), SAKINE);
		character_names.insert(SNAME("TETO"), TETO);
	}
	HashMap<StringName, DIVACharacter>::Iterator i = character_names.find(p_key);
	if (i == character_names.end()) {
		return INVALID;
	}
	return i->value;
}

void ModuleTable::parse(const String &p_text, const Ref<DIVASpriteDB> &p_sprite_db) {
	KVTable table;
	table.parse(p_text);

	const static StringName module_sname = "module";
	const static StringName id_sname = "id";
	const static StringName sort_index_sname = "sort_index";
	const static StringName name_sname = "name";
	const static StringName chara_sname = "chara";
	const static StringName cos_sname = "cos";

	ERR_FAIL_COND(!table.has_key(module_sname));

	for (int i = 0; i < table.get_children_count(module_sname); i++) {
		DivaModule module;

		if (table.child_has_key(module_sname, i, id_sname)) {
			module.id = table.child_get_value(module_sname, i, id_sname).to_int();
		}

		if (table.child_has_key(module_sname, i, sort_index_sname)) {
			module.sort_index = table.child_get_value(module_sname, i, sort_index_sname).to_int();
		}

		if (table.child_has_key(module_sname, i, name_sname)) {
			module.name = table.child_get_value(module_sname, i, name_sname);
		}

		if (table.child_has_key(module_sname, i, chara_sname)) {
			module.chara = get_character(StringName(table.child_get_value(module_sname, i, chara_sname)));
		}

		if (table.child_has_key(module_sname, i, cos_sname)) {
			String cos_str = table.child_get_value(module_sname, i, cos_sname);
			if (sscanf(cos_str.utf8().get_data(), "COS_%03d", &module.cos) == 1) {
				module.cos--;
			}
		}

		StringName modules_sprite_set_ids[ModuleSprite::MODULE_SPRITE_MAX]{
			vformat("SPR_SEL_MD%03d", module.id),
			vformat("SPR_SEL_MD%03dCMN", module.id),
		};

		StringName modules_sprite_ids[ModuleSprite::MODULE_SPRITE_MAX]{
			vformat("SPR_SEL_MD%03d_MD_IMG_%03d", module.id, module.id),
			vformat("SPR_SEL_MD%03dCMN_MD_IMG", module.id)
		};

		module.module_sprite_sets[ModuleSprite::SEL_MD] = p_sprite_db->get_sprite_set_idx_by_name(modules_sprite_set_ids[ModuleSprite::SEL_MD]);
		module.module_sprite_sets[ModuleSprite::SEL_MD_CMN] = p_sprite_db->get_sprite_set_idx_by_name(modules_sprite_set_ids[ModuleSprite::SEL_MD_CMN]);

		module.module_sprite_sets[ModuleSprite::SEL_MD] = p_sprite_db->get_sprite_set_idx_by_name(modules_sprite_set_ids[ModuleSprite::SEL_MD]);
		module.module_sprite_sets[ModuleSprite::SEL_MD_CMN] = p_sprite_db->get_sprite_set_idx_by_name(modules_sprite_set_ids[ModuleSprite::SEL_MD_CMN]);

		modules.insert(module.id, module);
	}
}
