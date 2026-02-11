#ifndef SPRITE_DB_H
#define SPRITE_DB_H

#include "core/io/json.h"
#include "read_helpers.h"
#include "sprite_set.h"

class DIVASpriteDB : public RefCounted {
	GDCLASS(DIVASpriteDB, RefCounted);
	struct SpriteInfo {
		uint32_t id;
		StringName name;
		uint16_t index;
		bool texture;
	};
	struct SpriteSetInfo {
		uint32_t id;
		String name;
		String file_name;
		HashMap<StringName, SpriteInfo> sprites;
		uint32_t index;
	};

	LocalVector<SpriteSetInfo> sprite_sets;
	HashMap<StringName, int> sprite_sets_by_name;

public:
	void read_classic(Ref<StreamPeerBuffer> p_stream);
	void dump_json(const String &p_path);
	int get_sprite_set_idx_by_name(const StringName &p_name) const;
	int get_sprite_idx_by_name(int p_set_idx, const StringName &p_name) const;
	Ref<DIVASpriteSet> load_sprite_set(int p_set_idx) const;
};

#endif // SPRITE_DB_H
