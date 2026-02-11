#include "motion_db.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "read_helpers.h"

void DIVAMotionDB::read(Ref<StreamPeerBuffer> p_stream) {
	ERR_FAIL_COND_MSG(p_stream->get_u32() != 0x1, "DIVA motion DB had incorrect magic number");
	struct {
		uint32_t motion_sets_offset;
		uint32_t motion_set_ids_offset;
		uint32_t motion_set_count;
		uint32_t bone_name_offsets_offset;
		uint32_t bone_name_count;
	} header{
		.motion_sets_offset = p_stream->get_u32(),
		.motion_set_ids_offset = p_stream->get_u32(),
		.motion_set_count = p_stream->get_u32(),
		.bone_name_offsets_offset = p_stream->get_u32(),
		.bone_name_count = p_stream->get_u32()
	};

	motion_set_infos.resize(header.motion_set_count);

	DIVAReadHelpers::OffsetQueue offset_queue{
		.spb = p_stream,
	};

	offset_queue.position_push(header.motion_sets_offset);
	MotionSetInfo *mot_set = motion_set_infos.ptr();
	for (int i = 0; i < header.motion_set_count; i++) {
		uint32_t name_offset = p_stream->get_u32();
		uint32_t motion_name_offsets_offset = p_stream->get_u32();
		uint32_t motion_count = p_stream->get_u32();
		uint32_t motion_ids_offset = p_stream->get_u32();

		mot_set[i].name = DIVAReadHelpers::read_null_terminated_string(name_offset, offset_queue);
		mot_set[i].motions.resize(motion_count);

		MotionInfo *mot_infos = mot_set[i].motions.ptr();

		offset_queue.position_push(motion_name_offsets_offset);
		for (int j = 0; j < motion_count; j++) {
			uint32_t string_pos = p_stream->get_u32();
			mot_infos[j].name = DIVAReadHelpers::read_null_terminated_string(string_pos, offset_queue);
		}
		offset_queue.position_pop();

		offset_queue.position_push(motion_ids_offset);
		for (int j = 0; j < motion_count; j++) {
			mot_infos[j].id = p_stream->get_u32();
		}
		offset_queue.position_pop();
	}
	offset_queue.position_pop();

	offset_queue.position_push(header.motion_set_ids_offset);
	for (int i = 0; i < header.motion_set_count; i++) {
		mot_set[i].id = p_stream->get_u32();
	}
	offset_queue.position_pop();

	bone_names.resize(header.bone_name_count);
	offset_queue.position_push(header.bone_name_offsets_offset);
	String *bone_names_ptr = bone_names.ptr();
	for (int i = 0; i < header.bone_name_count; i++) {
		uint32_t string_pos = p_stream->get_u32();
		bone_names_ptr[i] = DIVAReadHelpers::read_null_terminated_string(string_pos, offset_queue);
	}
	offset_queue.position_pop();
}

void DIVAMotionDB::dump_json(const String &p_path) const {
	Dictionary dump;

	Array bone_names_arr;
	for (String bone_name : bone_names) {
		bone_names_arr.push_back(bone_name);
	}
	Array motion_set_infos_arr;
	for (const MotionSetInfo &motset : motion_set_infos) {
		Dictionary motion_set_dict;
		motion_set_dict["id"] = motset.id;
		motion_set_dict["name"] = motset.name;
		Array motion_infos_arr;

		for (const MotionInfo &motinfo : motset.motions) {
			Dictionary motinfo_dict;
			motinfo_dict["name"] = motinfo.name;
			motinfo_dict["id"] = motinfo.id;
			motion_infos_arr.push_back(motinfo_dict);
		}

		motion_set_dict["motions"] = motion_infos_arr;
		motion_set_infos_arr.push_back(motion_set_dict);
	}
	dump["bone_names"] = bone_names_arr;
	dump["motion_set_infos"] = motion_set_infos_arr;

	String out_str = JSON::stringify(dump, " ", false);

	Ref<FileAccess> fa = FileAccess::open(p_path, FileAccess::WRITE);
	fa->store_string(out_str);
}
