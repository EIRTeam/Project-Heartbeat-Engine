#ifndef OBJECT_DB_H
#define OBJECT_DB_H

#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/io/stream_peer.h"
#include "core/object/ref_counted.h"
#include "read_helpers.h"

class DIVAObjectDB : public RefCounted {
	GDCLASS(DIVAObjectDB, RefCounted);

	struct DIVAObject {
		uint32_t id;
		String name;
	};

	struct ObjectSet {
		String name;
		uint32_t id;
		String object_file_name;
		String texture_file_name;
		String archive_file_name;
		LocalVector<DIVAObject> objects;
	};

	LocalVector<ObjectSet> object_sets;
	HashMap<uint32_t, int> object_set_id_map;

public:
	void read_classic(Ref<StreamPeerBuffer> p_spb) {
		DIVAReadHelpers::OffsetQueue queue{
			.spb = p_spb
		};

		uint32_t object_set_count = p_spb->get_u32();
		uint32_t max_object_set_id = p_spb->get_u32();
		uint32_t object_sets_offset = p_spb->get_u32();
		uint32_t object_count = p_spb->get_u32();
		uint32_t objects_offset = p_spb->get_u32();

		object_sets.resize(object_set_count);

		queue.position_push(object_sets_offset);
		for (uint32_t i = 0; i < object_set_count; i++) {
			ObjectSet &set = object_sets[i];
			set.name = DIVAReadHelpers::read_null_terminated_string(p_spb->get_u32(), queue);
			set.id = p_spb->get_u32();
			set.object_file_name = DIVAReadHelpers::read_null_terminated_string(p_spb->get_u32(), queue);
			set.texture_file_name = DIVAReadHelpers::read_null_terminated_string(p_spb->get_u32(), queue);
			set.archive_file_name = DIVAReadHelpers::read_null_terminated_string(p_spb->get_u32(), queue);
			// ???
			p_spb->seek(p_spb->get_position() + 0x10);
			object_set_id_map.insert(set.id, i);
		}
		queue.position_pop();

		queue.position_push(objects_offset);
		for (uint32_t i = 0; i < object_count; i++) {
			uint32_t id = p_spb->get_u16();
			uint32_t set_id = p_spb->get_u16();
			uint32_t name_offset = p_spb->get_u32();
			DIVAObject object = {
				.id = id,
				.name = DIVAReadHelpers::read_null_terminated_string(name_offset, queue)
			};
			HashMap<uint32_t, int>::Iterator it = object_set_id_map.find(set_id);
			if (it == object_set_id_map.end()) {
				continue;
			}
			object_sets[it->value].objects.push_back(object);
		}
		queue.position_pop();
	}

	void dump_json(const String &p_path) {
		Array obj_sets_out;

		for (const ObjectSet &obj_set : object_sets) {
			Dictionary obj_set_out;
			obj_set_out["name"] = obj_set.name;
			obj_set_out["id"] = obj_set.id;
			obj_set_out["object_file_name"] = obj_set.object_file_name;
			obj_set_out["texture_file_name"] = obj_set.texture_file_name;
			obj_set_out["archive_file_name"] = obj_set.archive_file_name;

			Array objects;

			for (const DIVAObject &obj : obj_set.objects) {
				Dictionary obj_o;
				obj_o["id"] = obj.id;
				obj_o["name"] = obj.name;
				objects.push_back(obj_o);
			}

			obj_set_out["objects"] = objects;
			obj_sets_out.push_back(obj_set_out);
		}

		Dictionary out;
		out["object_sets"] = obj_sets_out;

		String json_out = JSON::stringify(out, " ", false);
		Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE);
		f->store_string(json_out);
	}
};

#endif // OBJECT_DB_H
