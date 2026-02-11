#ifndef MOTION_DB_H
#define MOTION_DB_H

#include "core/io/stream_peer.h"
#include "core/string/ustring.h"
#include "core/templates/local_vector.h"

class DIVAMotionDB {
	struct MotionInfo {
		String name;
		uint32_t id;
	};
	struct MotionSetInfo {
		uint32_t id;
		String name;
		LocalVector<MotionInfo> motions;
	};

	LocalVector<MotionSetInfo> motion_set_infos;
	LocalVector<String> bone_names;

public:
	void read(Ref<StreamPeerBuffer> p_stream);
	void dump_json(const String &p_path) const;
};

#endif // MOTION_DB_H
