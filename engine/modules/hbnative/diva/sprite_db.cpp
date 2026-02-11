#include "sprite_db.h"

void DIVASpriteDB::read_classic(Ref<StreamPeerBuffer> p_stream) {
	uint32_t sprite_sets_count = p_stream->get_u32();
	uint32_t sprite_sets_offset = p_stream->get_u32();
	uint32_t sprites_count = p_stream->get_u32();
	uint32_t sprites_offset = p_stream->get_u32();

	sprite_sets.resize(sprite_sets_count);

	DIVAReadHelpers::OffsetQueue queue{
		.spb = p_stream
	};

	queue.position_push(sprite_sets_offset);
	for (uint32_t i = 0; i < sprite_sets_count; i++) {
		SpriteSetInfo *spr_set = &sprite_sets[i];
		spr_set->id = p_stream->get_u32();
		spr_set->name = DIVAReadHelpers::read_null_terminated_string(p_stream->get_u32(), queue);
		spr_set->file_name = DIVAReadHelpers::read_null_terminated_string(p_stream->get_u32(), queue);
		spr_set->index = p_stream->get_u32();
	}
	queue.position_pop();

	queue.position_push(sprites_offset);
	for (uint32_t i = 0; i < sprites_count; i++) {
		SpriteInfo sprite_info{
			.id = p_stream->get_u32(),
		};

		uint32_t name_offset = p_stream->get_u32();
		uint32_t info_bitfield = p_stream->get_u32();

		uint16_t index = (uint16_t)(info_bitfield & 0xFFFF);
		uint16_t set_index = (uint16_t)((info_bitfield >> 16) & 0x0FFF);
		sprite_info.texture = !!((info_bitfield >> 16) & 0x1000);

		SpriteSetInfo *spr_set = &sprite_sets[set_index];
		sprite_info.name = StringName(DIVAReadHelpers::read_null_terminated_string(name_offset, queue));
		sprite_info.index = index;
		spr_set->sprites.insert(sprite_info.name, sprite_info);
	}
	queue.position_pop();
}

void DIVASpriteDB::dump_json(const String &p_path) {
	Dictionary out;
	Array sprite_sets_out;
	for (SpriteSetInfo &set_info : sprite_sets) {
		Dictionary sprite_set;
		sprite_set["id"] = set_info.id;
		sprite_set["file_name"] = set_info.file_name;
		sprite_set["index"] = set_info.index;
		sprite_set["name"] = set_info.name;

		Array sprites;

		for (KeyValue<StringName, SpriteInfo> &kv : set_info.sprites) {
			Dictionary spr_data;
			SpriteInfo &spr_info = kv.value;
			spr_data["id"] = spr_info.id;
			spr_data["index"] = spr_info.index;
			spr_data["name"] = spr_info.name;
			spr_data["texture"] = spr_info.texture;

			sprites.push_back(spr_data);
		}
		sprite_set["sprites"] = sprites;
		sprite_sets_out.push_back(sprite_set);
	}
	out["sprite_sets"] = sprite_sets_out;
	String json_out = JSON::stringify(out, " ", false);
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE);
	f->store_string(json_out);
}

int DIVASpriteDB::get_sprite_set_idx_by_name(const StringName &p_name) const {
	HashMap<StringName, int>::ConstIterator it = sprite_sets_by_name.find(p_name);
	if (it == sprite_sets_by_name.end()) {
		return -1;
	}
	return it->value;
}

int DIVASpriteDB::get_sprite_idx_by_name(int p_set_idx, const StringName &p_name) const {
	ERR_FAIL_INDEX_V(p_set_idx, sprite_sets.size(), -1);

	const SpriteSetInfo &set_info = sprite_sets[p_set_idx];

	for (uint32_t i = 0; i < set_info.sprites.size(); i++) {
		if (set_info.name == p_name) {
			return i;
		}
	}
	return -1;
}
