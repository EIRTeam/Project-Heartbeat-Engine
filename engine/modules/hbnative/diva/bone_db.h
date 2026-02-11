#ifndef BONE_DB_H
#define BONE_DB_H

#include "core/io/stream_peer.h"
#include "core/math/vector3.h"
#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/templates/local_vector.h"

class DIVABoneDB;
class Skeleton3D;

class DIVASkeleton : public RefCounted {
	GDCLASS(DIVASkeleton, RefCounted);
	enum BoneType {
		BONE_DATABASE_BONE_ROTATION = 0x00,
		BONE_DATABASE_BONE_TYPE_1 = 0x01,
		BONE_DATABASE_BONE_POSITION = 0x02,
		BONE_DATABASE_BONE_POSITION_ROTATION = 0x03,
		BONE_DATABASE_BONE_HEAD_IK_ROTATION = 0x04,
		BONE_DATABASE_BONE_ARM_IK_ROTATION = 0x05,
		BONE_DATABASE_BONE_LEGS_IK_ROTATION = 0x06,
	};

	enum SkeletonType {
		BONE_DATABASE_SKELETON_COMMON = 0,
		BONE_DATABASE_SKELETON_MIKU = 1,
		BONE_DATABASE_SKELETON_KAITO = 2,
		BONE_DATABASE_SKELETON_LEN = 3,
		BONE_DATABASE_SKELETON_LUKA = 4,
		BONE_DATABASE_SKELETON_MEIKO = 5,
		BONE_DATABASE_SKELETON_RIN = 6,
		BONE_DATABASE_SKELETON_HAKU = 7,
		BONE_DATABASE_SKELETON_NERU = 8,
		BONE_DATABASE_SKELETON_SAKINE = 9,
		BONE_DATABASE_SKELETON_TETO = 10,
		BONE_DATABASE_SKELETON_NONE = -1,
	};

	struct bone_database_bone {
		BoneType type;
		bool has_parent;
		uint8_t parent;
		uint8_t pole_target;
		uint8_t mirror;
		uint8_t flags;
		String name;
	};

	String name;
	LocalVector<bone_database_bone> bones;
	LocalVector<Vector3> positions;
	LocalVector<String> object_bone_names;
	LocalVector<String> motion_bone_names;
	LocalVector<uint16_t> bone_parents;
	float heel_height;

protected:
	static void _bind_methods();

public:
	void apply_to_skeleton(Skeleton3D *p_skel);

	friend class DIVABoneDB;
};

class DIVABoneDB : public RefCounted {
	GDCLASS(DIVABoneDB, RefCounted);
	LocalVector<Ref<DIVASkeleton>> skeletons;

protected:
	static void _bind_methods();

public:
	Ref<DIVASkeleton> get_skeleton(const String &p_name) const;
	void read_classic(Ref<StreamPeerBuffer> p_stream);
	void dump_json(const String &p_path) const;
};

#endif // BONE_DB_H
