#include "bone_db.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "read_helpers.h"
#include "scene/3d/skeleton_3d.h"

void DIVABoneDB::_bind_methods() {
	ClassDB::bind_method(D_METHOD("read_classic", "stream"), &DIVABoneDB::read_classic);
	ClassDB::bind_method(D_METHOD("dump_json", "path"), &DIVABoneDB::dump_json);
	ClassDB::bind_method(D_METHOD("get_skeleton", "name"), &DIVABoneDB::get_skeleton);
}

Ref<DIVASkeleton> DIVABoneDB::get_skeleton(const String &p_name) const {
	for (const Ref<DIVASkeleton> &skel : skeletons) {
		if (skel->name == p_name) {
			return skel;
		}
	}
	return nullptr;
}

void DIVABoneDB::read_classic(Ref<StreamPeerBuffer> p_stream) {
	uint32_t signature = p_stream->get_u32();

	ERR_FAIL_COND_MSG(signature != 0x09102720, "Bone database magic number invalid!");

	uint32_t skeleton_count = p_stream->get_u32();
	uint32_t skeleton_offsets_offset = p_stream->get_u32();
	uint32_t skeleton_name_offsets_offset = p_stream->get_u32();
	p_stream->seek(p_stream->get_position() + 0x14);

	skeletons.resize(skeleton_count);
	for (Ref<DIVASkeleton> &skel : skeletons) {
		skel.instantiate();
	}

	DIVAReadHelpers::OffsetQueue queue{
		.spb = p_stream
	};

	// Load skeletons
	queue.position_push(skeleton_offsets_offset);
	for (uint32_t i = 0; i < skeleton_count; i++) {
		Ref<DIVASkeleton> skel = skeletons[i];
		uint32_t skeleton_offset = p_stream->get_u32();

		queue.position_push(skeleton_offset);
		uint32_t bones_offset = p_stream->get_u32();
		uint32_t position_count = p_stream->get_u32();
		uint32_t positions_offset = p_stream->get_u32();
		uint32_t heel_height_offset = p_stream->get_u32();
		uint32_t object_bone_count = p_stream->get_u32();
		uint32_t object_bone_names_offset = p_stream->get_u32();
		uint32_t motion_bone_count = p_stream->get_u32();
		uint32_t motion_bone_names_offset = p_stream->get_u32();
		uint32_t parent_indices_offset = p_stream->get_u32();

		p_stream->seek(p_stream->get_position() + 0x14);

		// Obtain bone count
		uint32_t bone_count = 0;
		queue.position_push(bones_offset);
		while (true) {
			if (p_stream->get_u8() == 0xFF) {
				break;
			}
			p_stream->seek(p_stream->get_position() + 0x0B);
			bone_count++;
		}
		queue.position_pop();

		skel->bones.resize(bone_count);

		// Read bone data
		queue.position_push(bones_offset);
		for (uint32_t j = 0; j < bone_count; j++) {
			DIVASkeleton::bone_database_bone *bone = &skel->bones.ptr()[j];
			bone->type = (DIVASkeleton::BoneType)p_stream->get_u8();
			bone->has_parent = p_stream->get_u8();
			bone->parent = p_stream->get_u8();
			bone->pole_target = p_stream->get_u8();
			bone->mirror = p_stream->get_u8();
			bone->flags = p_stream->get_u8();
			p_stream->seek(p_stream->get_position() + 0x02);
			bone->name = DIVAReadHelpers::read_null_terminated_string(p_stream->get_u32(), queue);
		}

		queue.position_pop();

		// Bone positions buffer
		skel->positions.resize(position_count);

		queue.position_push(positions_offset);
		for (uint32_t j = 0; j < position_count; j++) {
			skel->positions[j] = Vector3(
					p_stream->get_float(),
					p_stream->get_float(),
					p_stream->get_float());
		}
		queue.position_pop();

		// heel height
		queue.position_push(heel_height_offset);
		skel->heel_height = p_stream->get_float();
		queue.position_pop();

		// Object bone (what even is this?)
		skel->object_bone_names.resize(object_bone_count);
		queue.position_push(object_bone_names_offset);
		for (uint32_t j = 0; j < object_bone_count; j++) {
			skel->object_bone_names[j] = DIVAReadHelpers::read_null_terminated_string(p_stream->get_u32(), queue);
		}
		queue.position_pop();

		// Motion bone names (not sure what this is either)
		skel->motion_bone_names.resize(motion_bone_count);
		queue.position_push(motion_bone_names_offset);
		for (uint32_t j = 0; j < motion_bone_count; j++) {
			skel->motion_bone_names[j] = DIVAReadHelpers::read_null_terminated_string(p_stream->get_u32(), queue);
		}
		queue.position_pop();

		// Parent indices
		skel->bone_parents.resize(motion_bone_count);
		queue.position_push(parent_indices_offset);
		p_stream->get_data((uint8_t *)skel->bone_parents.ptr(), motion_bone_count * sizeof(uint16_t));
		queue.position_pop();
		queue.position_pop();
	}
	queue.position_pop();
	queue.position_push(skeleton_name_offsets_offset);
	for (uint32_t i = 0; i < skeleton_count; i++)
		skeletons[i]->name = DIVAReadHelpers::read_null_terminated_string(p_stream->get_u32(), queue);
	queue.position_pop();
}

void DIVABoneDB::dump_json(const String &p_path) const {
	Array skeletons_array;

	for (const Ref<DIVASkeleton> &skel : skeletons) {
		Dictionary skeleton_out;
		skeleton_out["name"] = skel->name;

		if (skel->name == "MIK") {
			Array bone_datas;
			for (uint32_t i = 0; i < MIN(skel->bones.size(), 123213); i++) {
				Dictionary bone_dict;
				bone_dict["name"] = skel->bones[i].name;
				bone_dict["type"] = skel->bones[i].type;
				bone_dict["position"] = skel->positions[i];
				bone_dict["object_name"] = skel->object_bone_names[i];
				bone_dict["motion_bone_name"] = skel->motion_bone_names[i];
				bone_dict["has_parent"] = skel->bones[i].has_parent;
				bone_dict["parent"] = skel->bones[i].parent;
				bone_datas.push_back(bone_dict);
			}
			skeleton_out["bones"] = bone_datas;
			skeleton_out["heel_height"] = skel->heel_height;
		}

		skeletons_array.push_back(skeleton_out);
	}

	Dictionary out;
	out["skeletons"] = skeletons_array;

	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE);
	f->store_string(JSON::stringify(out, " ", false));
}

void DIVASkeleton::_bind_methods() {
	ClassDB::bind_method(D_METHOD("apply_to_skeleton", "skeleton"), &DIVASkeleton::apply_to_skeleton);
}

void DIVASkeleton::apply_to_skeleton(Skeleton3D *p_skel) {
	// Time to build a tree
	// We assume id 0 is root, I hope this is right lul

	p_skel->clear_bones();
	for (int i = 0; i < bones.size(); i++) {
		p_skel->add_bone(bones[i].name);
	}
	for (int i = 0; i < bones.size(); i++) {
		if (bones[i].type >= BONE_DATABASE_BONE_HEAD_IK_ROTATION || (bones[i].has_parent && bones[i].parent >= bones.size())) {
			continue;
		}
		if (bones[i].has_parent && bones[i].parent == i) {
			print_line("WTF", bones[i].parent, i, bones[i].name);
		}
		if (bones[i].has_parent) {
			p_skel->set_bone_parent(i, bones[i].parent);
		}
		Transform3D bone_rest;
		bone_rest.origin = positions[i];
		p_skel->set_bone_rest(i, bone_rest);
	}
}
