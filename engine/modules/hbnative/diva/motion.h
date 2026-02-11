#ifndef MOTION_H
#define MOTION_H

#include "core/io/stream_peer.h"

class DIVAMotion {
	enum mot_key_set_type {
		MOT_KEY_SET_NONE = 0x00,
		MOT_KEY_SET_STATIC = 0x01,
		MOT_KEY_SET_HERMITE = 0x02,
		MOT_KEY_SET_HERMITE_TANGENT = 0x03,
	};

	enum mot_key_set_data_type {
		MOT_KEY_SET_DATA_F32 = 0x00,
		MOT_KEY_SET_DATA_F16 = 0x01,
	};

	struct motion_header_classic {
		uint32_t key_set_info_offset;
		uint32_t key_set_types_offset;
		uint32_t key_set_offset;
		uint32_t bone_info_offset;
	};

	struct key_set_data {
		mot_key_set_type type;
		LocalVector<uint16_t> frames;
		LocalVector<float> values;
		uint16_t keys_count : 16;
		mot_key_set_data_type data_type : 16;
	};

	struct key_bone_info {
		uint16_t index;
	};

	struct motion_data {
		union {
			struct {
				uint16_t key_set_count : 14;
				uint16_t skeleton_select : 1;
				uint16_t high_bit : 1;
			};
			uint16_t info;
		};
		uint16_t frame_count;
		int32_t bone_info_count;
		LocalVector<key_bone_info> bone_info;
		LocalVector<key_set_data> key_sets;
	};

	static void align_read(Ref<StreamPeerBuffer> &p_stream, int align) {
		int64_t position = p_stream->get_position();
		size_t temp_align = align - position % align;
		if (align != temp_align) {
			p_stream->seek(position + temp_align);
		}
	}

	void read_classic(Ref<StreamPeerBuffer> p_stream) {
		motion_header_classic header{
			.key_set_info_offset = p_stream->get_u32(),
			.key_set_types_offset = p_stream->get_u32(),
			.key_set_offset = p_stream->get_u32(),
			.bone_info_offset = p_stream->get_u32()
		};

		p_stream->seek(header.key_set_info_offset);

		motion_data data;
		data.info = p_stream->get_u16();
		data.frame_count = p_stream->get_u16();

		// Bone info
		{
			p_stream->seek(header.bone_info_offset);
			uint32_t bone_info_count = 0;
			do {
				bone_info_count++;
			} while (p_stream->get_u16() != 0 && p_stream->get_position() < p_stream->get_size());

			data.bone_info.resize(bone_info_count);

			p_stream->seek(header.bone_info_offset);

			key_bone_info *bone_info = data.bone_info.ptr();
			for (int i = 0; i < data.bone_info_count; i++) {
				bone_info[i].index = p_stream->get_u16();
			}
		}

		uint32_t key_set_count = data.key_set_count;
		key_set_data *key_set_arr = data.key_sets.ptr();

		// Key set type
		{
			data.key_sets.resize(key_set_count);

			p_stream->seek(header.key_set_types_offset);

			for (int32_t j = 0, b = 0; j < data.key_set_count; j++) {
				if (!(j % 8))
					b = p_stream->get_u16();

				key_set_arr[j].type = (mot_key_set_type)((b >> (j % 8 * 2)) & 0x03);
			}
		}

		{
			p_stream->seek(header.key_set_offset);

			for (uint32_t i = 0; i < key_set_count; i++) {
				key_set_data *ks_data = &key_set_arr[i];
				if (ks_data->type == MOT_KEY_SET_NONE) {
					ks_data->keys_count = 0;
					ks_data->frames.clear();
					ks_data->values.clear();
				} else if (ks_data->type == MOT_KEY_SET_STATIC) {
					ks_data->keys_count = 1;
					ks_data->frames.clear();
					ks_data->values.clear();
					ks_data->values.push_back(p_stream->get_float());
				} else {
					bool has_tangents = ks_data->type != MOT_KEY_SET_HERMITE;
					uint16_t keys_count = p_stream->get_u16();
					ks_data->keys_count = keys_count;

					ks_data->frames.resize(keys_count);
					ks_data->values.resize(has_tangents ? keys_count * 2ULL : keys_count);

					p_stream->get_data((uint8_t *)(ks_data->frames.ptr()), sizeof(uint16_t) * keys_count);

					align_read(p_stream, 0x04);

					if (!has_tangents) {
						p_stream->get_data((uint8_t *)ks_data->values.ptr(), sizeof(float_t) * keys_count);
					} else {
						p_stream->get_data((uint8_t *)ks_data->values.ptr(), sizeof(float_t) * keys_count * 2ULL);
					}
				}
			}
		}
	}
};

#endif // MOTION_H
