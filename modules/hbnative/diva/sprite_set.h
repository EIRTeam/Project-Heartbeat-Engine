#ifndef SPRITE_SET_H
#define SPRITE_SET_H

#include "core/error/error_macros.h"
#include "core/io/json.h"
#include "core/io/stream_peer.h"
#include "read_helpers.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/texture.h"

class DivaTXP : public RefCounted {
	enum DIVATextureFormat {
		DIVA_A8 = 0,
		DIVA_RGB8 = 1,
		DIVA_RGBA8 = 2,
		DIVA_RGB5 = 3,
		DIVA_RGB5A1 = 4,
		DIVA_RGBA4 = 5,
		DIVA_BC1 = 6,
		DIVA_BC1a = 7,
		DIVA_BC2 = 8,
		DIVA_BC3 = 9,
		DIVA_BC4 = 10,
		DIVA_BC5 = 11,
		DIVA_L8 = 12,
		DIVA_L8A8 = 13,
	};

	static String diva_texture_format_to_str(DIVATextureFormat p_tex_format);

	struct DIVAMipmap {
		Size2i size;
		DIVATextureFormat format;
		uint32_t id;
		Vector<uint8_t> data;
	};
	struct DIVATexture {
		bool cube_map;
		uint32_t array_size;
		uint32_t mipmap_count;
		LocalVector<DIVAMipmap> mipmaps;
	};
	LocalVector<DIVATexture> textures;

public:
	static Image::Format diva_to_godot_format(DIVATextureFormat p_tex_format);
	/*static uint32_t get_diva_size(DIVATextureFormat p_format, Size2i p_size) {
		uint32_t size = p_size.width * p_size.height;
		switch (p_format) {
		case DIVA_A8:
			return size;
		case DIVA_RGB8:
			return size * 3;
		case DIVA_RGBA8:
			return size * 4;
		case DIVA_RGB5:
			return size * 2;
		case DIVA_RGB5A1:
			return size * 2;
		case DIVA_RGBA4:
			return size * 2;
		case DIVA_L8:
			return size;
		case DIVA_L8A8:
			return size * 2;
		case DIVA_BC1:
		case DIVA_BC1a:
		case DIVA_BC2:
		case DIVA_BC3:
		case DIVA_BC4:
		case DIVA_BC5:
			width = align_val(width, 4);
			height = align_val(height, 4);
			size = width * height;
			switch (format) {
			case TXP_BC1:
				return size / 2;
			case TXP_BC1a:
				return size / 2;
			case TXP_BC2:
				return size;
			case TXP_BC3:
				return size;
			case TXP_BC4:
				return size / 2;
			case TXP_BC5:
				return size;
			}
			break;
		}
		return 0;
	}*/

	Vector<Ref<Image>> get_texture_mipmaps(int p_idx) const;

	void read_classic(Ref<StreamPeerBuffer> p_spb);

	void dump_json(String p_path);
};

class DIVASpriteSet : public RefCounted {
	GDCLASS(DIVASpriteSet, RefCounted);
	struct SpriteInfo {
		uint32_t texture_id;
		int32_t rotate;
		Rect2 rect;
		Vector2 pos;
		Size2 size;
		StringName name;
		uint32_t attributes;
		// TODO: This
		uint32_t resolution_mode;
	};
	struct SpriteSetData {
		uint32_t flags;
		LocalVector<SpriteInfo> sprite_infos;
		Ref<DivaTXP> texture_set;
	};
	SpriteSetData set_data;
	Ref<StreamPeerBuffer> spb;

public:
	void read_classic(Ref<StreamPeerBuffer> p_spb);

	Ref<DivaTXP> get_texture_set() const {
		return set_data.texture_set;
	}
};

#endif // SPRITE_SET_H
