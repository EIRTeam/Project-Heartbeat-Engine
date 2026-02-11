#include "sprite_set.h"

#include "core/io/file_access.h"

String DivaTXP::diva_texture_format_to_str(DIVATextureFormat p_tex_format) {
	switch (p_tex_format) {
		case DIVA_A8: {
			return "A8";
		} break;
		case DIVA_RGB8: {
			return "RGB8";
		} break;
		case DIVA_RGBA8: {
			return "RGBA8";
		} break;
		case DIVA_RGB5: {
			return "RGB5";
		} break;
		case DIVA_RGB5A1: {
			return "RGB5A1";
		} break;
		case DIVA_RGBA4: {
			return "RGBA4";
		} break;
		case DIVA_BC1: {
			return "BC1";
		} break;
		case DIVA_BC1a: {
			return "BC1a";
		} break;
		case DIVA_BC2: {
			return "BC2";
		} break;
		case DIVA_BC3: {
			return "BC3";
		} break;
		case DIVA_BC4: {
			return "BC4";
		} break;
		case DIVA_BC5: {
			return "BC5";
		} break;
		case DIVA_L8: {
			return "L8";
		} break;
		case DIVA_L8A8: {
			return "L8A8";
		} break;
	}
	return "";
}
Image::Format DivaTXP::diva_to_godot_format(DIVATextureFormat p_tex_format) {
	switch (p_tex_format) {
		case DIVA_A8: {
			return Image::FORMAT_R8;
		} break;
		case DIVA_RGB8: {
			return Image::FORMAT_RGB8;
		} break;
		case DIVA_RGBA8: {
			return Image::FORMAT_RGBA8;
		} break;
		case DIVA_RGB5: {
			// Unsupported
			return Image::FORMAT_MAX;
		} break;
		case DIVA_RGB5A1: {
			// Unsupported
			return Image::FORMAT_MAX;
		} break;
		case DIVA_RGBA4: {
			return Image::FORMAT_RGBA4444;
		} break;
		case DIVA_BC1: {
			return Image::FORMAT_DXT1;
		} break;
		case DIVA_BC1a: {
			return Image::FORMAT_DXT1;
		} break;
		case DIVA_BC2: {
			return Image::FORMAT_DXT3;
		} break;
		case DIVA_BC3: {
			return Image::FORMAT_DXT5;
		} break;
		case DIVA_BC4: {
			// ATI1
			return Image::FORMAT_RGTC_R;
		} break;
		case DIVA_BC5: {
			// ATI2
			// Do note ati2 here doesn't *actually* mean ATI2, but rather two
			// textures combined as YA + CBCR
			return Image::FORMAT_RGTC_RG;
		} break;
		case DIVA_L8: {
			return Image::FORMAT_L8;
		} break;
		case DIVA_L8A8: {
			return Image::FORMAT_LA8;
		} break;
	}
	return Image::FORMAT_MAX;
}
Vector<Ref<Image>> DivaTXP::get_texture_mipmaps(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, textures.size(), Vector<Ref<Image>>());
	Vector<Ref<Image>> mipmaps;
	for (int i = 0; i < textures[p_idx].mipmaps.size(); i++) {
		Image::Format godot_format = diva_to_godot_format(textures[p_idx].mipmaps[i].format);
		const Vector<uint8_t> mipmap_data = textures[p_idx].mipmaps[i].data;
		Size2i mipmap_size = textures[p_idx].mipmaps[i].size;
		Ref<Image> img = Image::create_from_data(mipmap_size.x, mipmap_size.y, false, godot_format, mipmap_data);
		mipmaps.push_back(img);
	}
	return mipmaps;
}
void DivaTXP::read_classic(Ref<StreamPeerBuffer> p_spb) {
	uint32_t set_start = p_spb->get_position();
	uint32_t signature = p_spb->get_u32();
	ERR_FAIL_COND_MSG(signature != 0x03505854, "Texture set signature was wrong");

	DIVAReadHelpers::OffsetQueue queue{
		.spb = p_spb
	};

	uint32_t texture_count = p_spb->get_u32();

	textures.reserve(texture_count);

	// Not sure why but we have to skip 4 bytes here
	// Both MML and ReDIVA skip it, MML calls it "texture count with rubbish"
	p_spb->get_u32();
	for (uint32_t i = 0; i < texture_count; i++) {
		uint32_t texture_start = set_start + p_spb->get_u32();
		queue.position_push(texture_start);

		uint32_t txp_signature = p_spb->get_u32();
		if (txp_signature != 0x04505854 && txp_signature != 0x05505854) {
			continue;
		}

		uint32_t subtex_count = p_spb->get_u32();
		uint32_t tex_info = p_spb->get_u32();

		DIVATexture tex = {
			.cube_map = signature == 0x05505854,
			.array_size = (tex_info >> 8) & 0xFF,
			.mipmap_count = tex_info & 0xFF
		};

		if (tex.array_size == 1 && tex.mipmap_count != subtex_count) {
			tex.mipmap_count = subtex_count & 0xFF;
		}

		tex.mipmaps.resize(tex.array_size * tex.mipmap_count);

		for (uint32_t arr_i = 0; arr_i < tex.array_size; arr_i++) {
			for (uint32_t mipmap_i = 0; mipmap_i < tex.mipmap_count; mipmap_i++) {
				uint32_t mipmap_offset = p_spb->get_u32();
				queue.position_push(texture_start + mipmap_offset);

				uint32_t subtex_signature = p_spb->get_u32(); // Mipmap signature

				ERR_FAIL_COND_MSG(subtex_signature != 0x02505854, "Subtexture signature was incorrect");

				DIVAMipmap mipmap = {
					.size = Size2i(p_spb->get_u32(), p_spb->get_u32()),
					.format = (DIVATextureFormat)p_spb->get_u32(),
					.id = p_spb->get_u32()
				};

				uint32_t data_size = p_spb->get_u32();

				mipmap.data.resize(data_size);

				p_spb->get_data((uint8_t *)mipmap.data.ptr(), mipmap.data.size());

				tex.mipmaps[arr_i * tex.mipmap_count + mipmap_i] = mipmap;

				queue.position_pop();
			}
		}

		textures.push_back(tex);

		queue.position_pop();
	}
}
void DivaTXP::dump_json(String p_path) {
	Array textures_out;
	for (const DIVATexture &text : textures) {
		Dictionary text_out;
		text_out["cube_map"] = text.cube_map;
		text_out["array_size"] = text.array_size;

		Array mipmaps;
		for (const DIVAMipmap &mipmap : text.mipmaps) {
			Dictionary mipmap_out;
			mipmap_out["format"] = diva_texture_format_to_str(mipmap.format);
			mipmap_out["size"] = mipmap.size;
			mipmaps.push_back(mipmap_out);
		}
		text_out["mipmaps"] = mipmaps;
		textures_out.push_back(text_out);
	}

	Dictionary out;

	out["textures"] = textures_out;

	String json_out = JSON::stringify(out, " ", false);
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE);
	f->store_string(json_out);
}

void DIVASpriteSet::read_classic(Ref<StreamPeerBuffer> p_spb) {
	spb = p_spb;

	DIVAReadHelpers::OffsetQueue queue{
		.spb = p_spb
	};

	set_data.flags = p_spb->get_u32();
	uint32_t textures_offset = p_spb->get_u32();
	uint32_t texture_count = p_spb->get_u32();
	uint32_t sprite_count = p_spb->get_u32();
	uint32_t sprite_info_offset = p_spb->get_u32();
	uint32_t texture_name_offset = p_spb->get_u32();
	uint32_t sprite_name_offset = p_spb->get_u32();
	uint32_t sprite_data_offset = p_spb->get_u32();

	set_data.sprite_infos.resize(sprite_count);

	p_spb->seek(sprite_info_offset);
	for (uint32_t i = 0; i < sprite_count; i++) {
		SpriteInfo *spr_info = &set_data.sprite_infos[i];
		spr_info->texture_id = p_spb->get_u32();
		spr_info->rotate = p_spb->get_32();
		spr_info->rect = Rect2(Vector2(p_spb->get_float(), p_spb->get_float()), Vector2(p_spb->get_float(), p_spb->get_float()));
		spr_info->pos = Vector2(Vector2(p_spb->get_float(), p_spb->get_float()));
		spr_info->size = Vector2(Vector2(p_spb->get_float(), p_spb->get_float()));
	}

	p_spb->seek(sprite_name_offset);
	for (uint32_t i = 0; i < sprite_count; i++) {
		set_data.sprite_infos[i].name = DIVAReadHelpers::read_null_terminated_string(p_spb->get_u32(), queue);
	}
	p_spb->seek(sprite_data_offset);
	for (uint32_t i = 0; i < sprite_count; i++) {
		set_data.sprite_infos[i].attributes = p_spb->get_u32();
		set_data.sprite_infos[i].resolution_mode = p_spb->get_u32();
	}

	p_spb->seek(textures_offset);
	for (uint32_t i = 0; i < texture_count; i++) {
		set_data.texture_set.instantiate();
		set_data.texture_set->read_classic(p_spb);
	}
}
