#ifndef ITEM_TABLE_H
#define ITEM_TABLE_H

#include "core/io/stream_peer.h"
#include "kv_table.h"

class DIVASpriteDB;

enum DIVACharacter {
	MIKU,
	RIN,
	LEN,
	LUKA,
	NERU,
	HAKU,
	KAITO,
	MEIKO,
	SAKINE,
	TETO,
	INVALID = 9999
};

enum DIVAItemSlot {
	ITEM_NONE = -1,
	ITEM_BODY = 0x00,
	ITEM_ATAMA = 0x01,
	ITEM_KATA_R = 0x02,
	ITEM_MUNE = 0x03,
	ITEM_KATA_L = 0x04,
	ITEM_UDE_R = 0x05,
	ITEM_SENAKA = 0x06,
	ITEM_UDE_L = 0x07,
	ITEM_HARA = 0x08,
	ITEM_KOSI = 0x09,
	ITEM_TE_R = 0x0A,
	ITEM_TE_L = 0x0B,
	ITEM_MOMO = 0x0C,
	ITEM_SUNE = 0x0D,
	ITEM_ASI = 0x0E,
	ITEM_KAMI = 0x0F,
	ITEM_OUTER = 0x10,
	ITEM_PANTS = 0x11,
	ITEM_ZUJO = 0x12,
	ITEM_MEGANE = 0x13,
	ITEM_KUBI = 0x14,
	ITEM_JOHA_USHIRO = 0x15,
	ITEM_KUCHI = 0x16,
	ITEM_ITEM09 = 0x17,
	ITEM_ITEM10 = 0x18,
	ITEM_ITEM11 = 0x19,
	ITEM_ITEM12 = 0x1A,
	ITEM_ITEM13 = 0x1B,
	ITEM_ITEM14 = 0x1C,
	ITEM_ITEM15 = 0x1D,
	ITEM_ITEM16 = 0x1E,
	ITEM_MAX = 0x1F,

	ITEM_OSAGE_FIRST = ITEM_KAMI,
	ITEM_OSAGE_COUNT = (ITEM_MAX - ITEM_KAMI),
};

enum DIVAItemSubID {
	ITEM_SUB_NONE = -1,
	ITEM_SUB_ZUJO = 0x00,
	ITEM_SUB_KAMI = 0x01,
	ITEM_SUB_HITAI = 0x02,
	ITEM_SUB_ME = 0x03,
	ITEM_SUB_MEGANE = 0x04,
	ITEM_SUB_MIMI = 0x05,
	ITEM_SUB_KUCHI = 0x06,
	ITEM_SUB_MAKI = 0x07,
	ITEM_SUB_KUBI = 0x08,
	ITEM_SUB_INNER = 0x09,
	ITEM_SUB_OUTER = 0x0A,
	ITEM_SUB_KATA = 0x0B,
	ITEM_SUB_U_UDE = 0x0C,
	ITEM_SUB_L_UDE = 0x0D,
	ITEM_SUB_TE = 0x0E,
	ITEM_SUB_JOHA_MAE = 0x0F,
	ITEM_SUB_JOHA_USHIRO = 0x10,
	ITEM_SUB_BELT = 0x11,
	ITEM_SUB_KOSI = 0x12,
	ITEM_SUB_PANTS = 0x13,
	ITEM_SUB_ASI = 0x14,
	ITEM_SUB_SUNE = 0x15,
	ITEM_SUB_KUTSU = 0x16,
	ITEM_SUB_HADA = 0x17,
	ITEM_SUB_HEAD = 0x18,
	ITEM_SUB_MAX = 0x19,
};

class ModuleTable {
	enum ModuleSprite {
		SEL_MD,
		SEL_MD_CMN,
		MODULE_SPRITE_MAX,
	};

	struct DivaModule {
		int id = 0;
		int sort_index = 0;
		int cos = 0;
		String name;
		DIVACharacter chara = INVALID;
		int module_sprite_sets[ModuleSprite::MODULE_SPRITE_MAX] = {
			-1, -1
		};
		int module_sprites[ModuleSprite::MODULE_SPRITE_MAX] = {
			-1, -1
		};
	};

	HashMap<int, DivaModule> modules;

	static DIVACharacter get_character(const StringName &p_key);

	void parse(const String &p_text, const Ref<DIVASpriteDB> &p_sprite_db);
};

// Per-character item table
class DIVAItemTable : public RefCounted {
	struct DIVACostume {
		uint32_t id;
		LocalVector<uint32_t> item_numbers;
	};

	struct CustomeItemTextureOverride {
		StringName original;
		StringName replacement;
	};

	struct CustomeObjectInfo {
		DIVAItemSlot rpk;
		StringName uid;
	};

	struct ItemOffset {
		DIVAItemSubID sub_id;
		int32_t number;
		Vector3 pos;
		Vector3 rot;
		Vector3 scale;
	};

	struct CostumeItemInfo {
		uint32_t number;
		StringName name;
		LocalVector<StringName> objsets;
		LocalVector<CustomeItemTextureOverride> texture_overrides;
	};
	LocalVector<DIVACostume> costumes;
	LocalVector<CostumeItemInfo> item_infos;

	HashMap<uint32_t, int> item_number_to_idx;

public:
	void parse(const String p_text) {
		KVTable table;
		table.parse(p_text);

		static const StringName cos_sname = "cos";
		static const StringName item_sname = "item";
		static const StringName length_sname = "length";
		static const StringName obj_sname = "obj";
		static const StringName data_sname = "data";

		ERR_FAIL_COND_MSG(!table.has_key(cos_sname), "item table invalid");
		print_line("COSCOUNT", table.get_children_count(cos_sname), table.get_value("cos.length"));
		print_line("ITEMCOUNT", table.get_children_count(item_sname), table.get_value("item.length"));
		ERR_FAIL_COND_MSG(table.get_children_count(cos_sname) != (table.get_value("cos.length").to_int() + 1), "Costume count mismatch");
		ERR_FAIL_COND_MSG(table.get_children_count(item_sname) != (table.get_value("item.length").to_int() + 1), "Item count mismatch");
		for (int i = 0; i < table.get_children_count(cos_sname); i++) {
			if (table.child_has_key(cos_sname, i, length_sname)) {
				continue;
			}
		}

		item_infos.resize(table.get_children_count(item_sname));

		for (int i = 0; i < table.get_children_count(item_sname); i++) {
			if (table.child_has_key(item_sname, i, length_sname)) {
				continue;
			}

			// HACK-Y
			StringName data_path = table.child_get_path(item_sname, i, data_sname);

			CostumeItemInfo *item_info = &item_infos[i];
			//item_info->name = table.child_get_value(obj_sname, i, )
		}
	}
};

#endif // ITEM_TABLE_H
