#include "diva_object.h"
void DIVAObjectSet::read_submesh_indices(DIVASubmesh *p_submesh, uint32_t p_index_count, Ref<StreamPeerBuffer> p_spb) {
	bool tri_strip = p_submesh->primitive == OBJ_PRIMITIVE_TRIANGLE_STRIP;
	p_submesh->index_array.resize(p_index_count);

	switch (p_submesh->index_format) {
		case OBJ_INDEX_U8: {
			if (tri_strip) {
				for (uint32_t i = 0; i < p_index_count; i++) {
					uint8_t idx = p_spb->get_u8();
					p_submesh->index_array[i] = idx == 0xFF ? 0xFFFFFFFF : idx;
				}
			} else {
				for (uint32_t i = 0; i < p_index_count; i++) {
					p_submesh->index_array[i] = p_spb->get_u8();
				}
			}
		} break;
		case OBJ_INDEX_U16: {
			if (tri_strip) {
				for (uint32_t i = 0; i < p_index_count; i++) {
					uint16_t idx = p_spb->get_u16();
					p_submesh->index_array[i] = idx == 0xFFFF ? 0xFFFFFFFF : idx;
				}
			} else {
				for (uint32_t i = 0; i < p_index_count; i++) {
					p_submesh->index_array[i] = p_spb->get_u16();
				}
			}
		} break;
		case OBJ_INDEX_U32: {
			for (uint32_t i = 0; i < p_index_count; i++) {
				p_submesh->index_array[i] = p_spb->get_u32();
			}
		} break;
	}
}
void DIVAObjectSet::read_submesh(DIVASubmesh *p_submesh, Ref<StreamPeerBuffer> p_spb, uint32_t p_base_offset) {
	p_submesh->flags = p_spb->get_u32();
	p_submesh->bounding_sphere.center.x = p_spb->get_float();
	p_submesh->bounding_sphere.center.y = p_spb->get_float();
	p_submesh->bounding_sphere.center.z = p_spb->get_float();
	p_submesh->bounding_sphere.radius = p_spb->get_float();
	p_submesh->material = p_spb->get_u32();

	p_spb->get_data(p_submesh->uv_indices, 8);

	uint32_t bone_index_count = p_spb->get_u32();
	uint32_t bone_indices_offset = p_spb->get_u32();
	p_submesh->bones_per_vertex = p_spb->get_u32();

	p_submesh->primitive = (DIVAPrimitive)p_spb->get_u32();
	p_submesh->index_format = (DIVAIndexFormat)p_spb->get_u32();

	uint32_t index_count = p_spb->get_u32();
	uint32_t indices_offset = p_spb->get_u32();
	// ?
	p_spb->get_u32();
	p_submesh->bounding_box.center = p_submesh->bounding_sphere.center;
	p_submesh->bounding_box.size = Vector3(1.0f, 1.0f, 1.0f) * (p_submesh->bounding_sphere.radius * 2.0f);
	// ?
	p_spb->seek(p_spb->get_position() + 0x1C);

	if (p_submesh->bones_per_vertex == 4 && bone_indices_offset != 0) {
		p_submesh->bone_index_array.resize(bone_index_count);
		p_spb->seek(p_base_offset + bone_indices_offset);
		p_spb->get_data((uint8_t *)p_submesh->bone_index_array.ptr(), bone_index_count * sizeof(uint16_t));
	}

	p_spb->seek(p_base_offset + indices_offset);
	read_submesh_indices(p_submesh, index_count, p_spb);
}

void DIVAObjectSet::read_model_vertex_data_godot(DIVAMesh *p_mesh, Ref<StreamPeerBuffer> p_spb, uint32_t p_base_offset, uint32_t p_vertex_offsets[20], uint32_t p_vertex_count, uint32_t p_vertex_format) {
	p_mesh->vertices.resize(p_vertex_count);

	Array mesh_data;
	mesh_data.resize(ArrayMesh::ARRAY_MAX);

	BitField<ArrayMesh::ArrayFormat> mesh_format;
	mesh_format.clear();

	for (uint32_t i = 0; i < 20; i++) {
		DIVAVertexFormat attrib = (DIVAVertexFormat)(1 << i);
		if (!(p_vertex_format & attrib)) {
			continue;
		}

		p_spb->seek(p_base_offset + p_vertex_offsets[i]);

		switch (attrib) {
			case OBJ_VERTEX_FILE_POSITION: {
				PackedVector3Array positions;
				positions.resize(p_vertex_count);
				{
					Vector3 *positions_ptr = positions.ptrw();
					for (uint32_t j = 0; j < p_vertex_count; j++) {
						positions_ptr[j].x = p_spb->get_float();
						positions_ptr[j].y = p_spb->get_float();
						positions_ptr[j].z = p_spb->get_float();
					}
				}
				mesh_data[ArrayMesh::ARRAY_VERTEX] = positions;
				mesh_format.set_flag(ArrayMesh::ARRAY_FORMAT_VERTEX);
				p_mesh->godot_vertex_data.positions = positions;
			} break;
			case OBJ_VERTEX_FILE_NORMAL: {
				PackedVector3Array normals;
				normals.resize(p_vertex_count);
				{
					Vector3 *normals_ptr = normals.ptrw();
					for (uint32_t j = 0; j < p_vertex_count; j++) {
						normals_ptr[j].x = p_spb->get_float();
						normals_ptr[j].y = p_spb->get_float();
						normals_ptr[j].z = p_spb->get_float();
					}
				}
				mesh_data[ArrayMesh::ARRAY_NORMAL] = normals;
				mesh_format.set_flag(ArrayMesh::ARRAY_FORMAT_NORMAL);
				p_mesh->godot_vertex_data.normals = normals;
			} break;
			case OBJ_VERTEX_FILE_TANGENT: {
				PackedFloat32Array tangents;
				tangents.resize(p_vertex_count * 4);
				{
					float *tangents_ptr = tangents.ptrw();
					for (uint32_t j = 0; j < tangents.size(); j++) {
						tangents_ptr[j] = p_spb->get_float();
					}
				}
				mesh_data[ArrayMesh::ARRAY_TANGENT] = tangents;
				p_mesh->godot_vertex_data.tangents = tangents;
				p_mesh->godot_vertex_data.format.set_flag(ArrayMesh::ARRAY_FORMAT_TANGENT);
			} break;
			case OBJ_VERTEX_FILE_BINORMAL: {
				// Unsupported
				p_spb->seek(p_spb->get_position() + p_vertex_count * sizeof(float) * 3);
			} break;
			case OBJ_VERTEX_FILE_TEXCOORD0: {
				PackedVector2Array uvs;
				uvs.resize(p_vertex_count);
				Vector2 *uvs_ptr = uvs.ptrw();
				{
					for (uint32_t j = 0; j < p_vertex_count; j++) {
						uvs_ptr[j].x = p_spb->get_float();
						uvs_ptr[j].y = p_spb->get_float();
					}
				}
				p_mesh->godot_vertex_data.texcoord0 = uvs;
				mesh_data[ArrayMesh::ARRAY_TEX_UV] = uvs;
				p_mesh->godot_vertex_data.format.set_flag(ArrayMesh::ARRAY_FORMAT_TEX_UV);
			} break;
			case OBJ_VERTEX_FILE_TEXCOORD1: {
				PackedVector2Array uvs;
				uvs.resize(p_vertex_count);
				Vector2 *uvs_ptr = uvs.ptrw();
				{
					for (uint32_t j = 0; j < p_vertex_count; j++) {
						uvs_ptr[j].x = p_spb->get_float();
						uvs_ptr[j].y = p_spb->get_float();
					}
				}
				mesh_data[ArrayMesh::ARRAY_TEX_UV2] = uvs;
				p_mesh->godot_vertex_data.texcoord1 = uvs;
				p_mesh->godot_vertex_data.format.set_flag(ArrayMesh::ARRAY_FORMAT_TEX_UV2);
			} break;
			case OBJ_VERTEX_FILE_TEXCOORD2: {
				// Unsupported
				p_spb->seek(p_spb->get_position() + p_vertex_count * sizeof(float) * 2);
			} break;
			case OBJ_VERTEX_FILE_TEXCOORD3: {
				// Unsupported
				p_spb->seek(p_spb->get_position() + p_vertex_count * sizeof(float) * 2);
			} break;
			case OBJ_VERTEX_FILE_COLOR0: {
				PackedColorArray colors;
				colors.resize(p_vertex_count);
				{
					Color *color_ptr = colors.ptrw();
					for (uint32_t j = 0; j < p_vertex_count; j++) {
						color_ptr[j].r = p_spb->get_float();
						color_ptr[j].g = p_spb->get_float();
						color_ptr[j].b = p_spb->get_float();
						color_ptr[j].a = p_spb->get_float();
					}
				}
				mesh_data[ArrayMesh::ARRAY_COLOR] = colors;
				p_mesh->godot_vertex_data.color0 = colors;
				p_mesh->godot_vertex_data.format.set_flag(ArrayMesh::ARRAY_FORMAT_COLOR);
			} break;
			case OBJ_VERTEX_FILE_COLOR1: {
				p_spb->seek(p_spb->get_position() + p_vertex_count * sizeof(float) * 4);
			} break;
			case OBJ_VERTEX_FILE_BONE_WEIGHT: {
				PackedFloat32Array bone_weights;
				bone_weights.resize(p_vertex_count);
				{
					float *bone_weights_ptr = bone_weights.ptrw();
					for (uint32_t j = 0; j < bone_weights.size(); j++) {
						bone_weights_ptr[j] = p_spb->get_float();
					}
				}
				mesh_data[ArrayMesh::ARRAY_WEIGHTS] = bone_weights;
				p_mesh->godot_vertex_data.bone_weights = bone_weights;
				p_mesh->godot_vertex_data.format.set_flag(ArrayMesh::ARRAY_FORMAT_WEIGHTS);
			} break;
			case OBJ_VERTEX_FILE_BONE_INDEX: {
				PackedInt32Array bone_indices;
				bone_indices.resize(p_vertex_count * 4);
				{
					int *bone_indices_ptr = bone_indices.ptrw();
					for (uint32_t j = 0; j < bone_indices.size(); j++) {
						int32_t bone_index_0 = (int32_t)p_spb->get_float();

						bone_indices_ptr[j] = (int16_t)(bone_index_0 >= 0 ? bone_index_0 / 3 : -1);
					}
				}
				mesh_data[ArrayMesh::ARRAY_BONES] = bone_indices;
				p_mesh->godot_vertex_data.bone_indices = bone_indices;
				p_mesh->godot_vertex_data.format.set_flag(ArrayMesh::ARRAY_FORMAT_WEIGHTS);
			}
			case OBJ_VERTEX_FILE_UNKNOWN: {
				for (uint32_t j = 0; j < p_vertex_count; j++) {
					p_spb->get_float();
					p_spb->get_float();
					p_spb->get_float();
					p_spb->get_float();
				}
			}
			case OBJ_VERTEX_FILE_MODERN_STORAGE:
				break;
		}
	}
	p_mesh->godot_vertex_data.format = mesh_format;
}

void DIVAObjectSet::read_model_vertex_data(DIVAMesh *p_mesh, Ref<StreamPeerBuffer> p_spb, uint32_t p_base_offset, uint32_t p_vertex_offsets[20], uint32_t p_vertex_count, uint32_t p_vertex_format) {
	p_mesh->vertices.resize(p_vertex_count);
	for (uint32_t i = 0; i < 20; i++) {
		DIVAVertexFormat attrib = (DIVAVertexFormat)(1 << i);
		if (!(p_vertex_format & attrib)) {
			continue;
		}

		p_spb->seek(p_base_offset + p_vertex_offsets[i]);

		switch (attrib) {
			case OBJ_VERTEX_FILE_POSITION: {
				for (uint32_t j = 0; j < p_vertex_count; j++) {
					p_mesh->vertices[j].position.x = p_spb->get_float();
					p_mesh->vertices[j].position.y = p_spb->get_float();
					p_mesh->vertices[j].position.z = p_spb->get_float();
				}
			} break;
			case OBJ_VERTEX_FILE_NORMAL: {
				for (uint32_t j = 0; j < p_vertex_count; j++) {
					p_mesh->vertices[j].normal.x = p_spb->get_float();
					p_mesh->vertices[j].normal.y = p_spb->get_float();
					p_mesh->vertices[j].normal.z = p_spb->get_float();
				}
			} break;
			case OBJ_VERTEX_FILE_TANGENT: {
				for (uint32_t j = 0; j < p_vertex_count; j++) {
					p_mesh->vertices[j].tangent.x = p_spb->get_float();
					p_mesh->vertices[j].tangent.y = p_spb->get_float();
					p_mesh->vertices[j].tangent.z = p_spb->get_float();
					p_mesh->vertices[j].tangent.w = p_spb->get_float();
				}
			} break;
			case OBJ_VERTEX_FILE_BINORMAL: {
				for (uint32_t j = 0; j < p_vertex_count; j++) {
					p_mesh->vertices[j].binormal.x = p_spb->get_float();
					p_mesh->vertices[j].binormal.y = p_spb->get_float();
					p_mesh->vertices[j].binormal.z = p_spb->get_float();
				}
			} break;
			case OBJ_VERTEX_FILE_TEXCOORD0: {
				for (uint32_t j = 0; j < p_vertex_count; j++) {
					p_mesh->vertices[j].texcoord0.x = p_spb->get_float();
					p_mesh->vertices[j].texcoord0.y = p_spb->get_float();
				}
			} break;
			case OBJ_VERTEX_FILE_TEXCOORD1: {
				for (uint32_t j = 0; j < p_vertex_count; j++) {
					p_mesh->vertices[j].texcoord1.x = p_spb->get_float();
					p_mesh->vertices[j].texcoord1.y = p_spb->get_float();
				}
			} break;
			case OBJ_VERTEX_FILE_TEXCOORD2: {
				for (uint32_t j = 0; j < p_vertex_count; j++) {
					p_mesh->vertices[j].texcoord2.x = p_spb->get_float();
					p_mesh->vertices[j].texcoord2.y = p_spb->get_float();
				}
			} break;
			case OBJ_VERTEX_FILE_TEXCOORD3: {
				for (uint32_t j = 0; j < p_vertex_count; j++) {
					p_mesh->vertices[j].texcoord3.x = p_spb->get_float();
					p_mesh->vertices[j].texcoord3.y = p_spb->get_float();
				}
			} break;
			case OBJ_VERTEX_FILE_COLOR0: {
				for (uint32_t j = 0; j < p_vertex_count; j++) {
					p_mesh->vertices[j].color0.r = p_spb->get_float();
					p_mesh->vertices[j].color0.g = p_spb->get_float();
					p_mesh->vertices[j].color0.b = p_spb->get_float();
					p_mesh->vertices[j].color0.a = p_spb->get_float();
				}
			}
			case OBJ_VERTEX_FILE_COLOR1: {
				for (uint32_t j = 0; j < p_vertex_count; j++) {
					p_mesh->vertices[j].color1.r = p_spb->get_float();
					p_mesh->vertices[j].color1.g = p_spb->get_float();
					p_mesh->vertices[j].color1.b = p_spb->get_float();
					p_mesh->vertices[j].color1.a = p_spb->get_float();
				}
			} break;
			case OBJ_VERTEX_FILE_BONE_WEIGHT: {
				for (uint32_t j = 0; j < p_vertex_count; j++) {
					p_mesh->vertices[j].bone_weights[0] = p_spb->get_float();
					p_mesh->vertices[j].bone_weights[1] = p_spb->get_float();
					p_mesh->vertices[j].bone_weights[2] = p_spb->get_float();
					p_mesh->vertices[j].bone_weights[3] = p_spb->get_float();
				}
			} break;
			case OBJ_VERTEX_FILE_BONE_INDEX: {
				for (uint32_t j = 0; j < p_vertex_count; j++) {
					int32_t bone_index_0 = (int32_t)p_spb->get_float();
					int32_t bone_index_1 = (int32_t)p_spb->get_float();
					int32_t bone_index_2 = (int32_t)p_spb->get_float();
					int32_t bone_index_3 = (int32_t)p_spb->get_float();

					p_mesh->vertices[j].bone_indices[0] = (int16_t)(bone_index_0 >= 0 ? bone_index_0 / 3 : -1);
					p_mesh->vertices[j].bone_indices[1] = (int16_t)(bone_index_1 >= 0 ? bone_index_1 / 3 : -1);
					p_mesh->vertices[j].bone_indices[2] = (int16_t)(bone_index_2 >= 0 ? bone_index_2 / 3 : -1);
					p_mesh->vertices[j].bone_indices[3] = (int16_t)(bone_index_3 >= 0 ? bone_index_3 / 3 : -1);
				}
			}
			case OBJ_VERTEX_FILE_UNKNOWN: {
				for (uint32_t j = 0; j < p_vertex_count; j++) {
					p_spb->get_float();
					p_spb->get_float();
					p_spb->get_float();
					p_spb->get_float();
				}
			}
			case OBJ_VERTEX_FILE_MODERN_STORAGE:
				break;
		}
	}
}
void DIVAObjectSet::read_mesh(DIVAMesh *p_mesh, Ref<StreamPeerBuffer> p_spb, uint32_t p_base_offset) {
	const size_t sub_mesh_size = 0x5C;

	p_mesh->flags = p_spb->get_u32();
	p_mesh->bounding_sphere.center.x = p_spb->get_float();
	p_mesh->bounding_sphere.center.y = p_spb->get_float();
	p_mesh->bounding_sphere.center.z = p_spb->get_float();
	p_mesh->bounding_sphere.radius = p_spb->get_float();

	uint32_t submesh_count = p_spb->get_u32();
	uint32_t submesh_offset = p_spb->get_u32();
	BitField<DIVAVertexFormat> vertex_format = (DIVAVertexFormat)p_spb->get_u32();
	p_spb->get_u32(); // Vertex size
	uint32_t vertex_count = p_spb->get_u32();

	// 20 vertex offsets
	uint32_t vertex_offsets[20];
	for (int i = 0; i < 20; i++) {
		vertex_offsets[i] = p_spb->get_u32();
	}

	p_spb->get_u32(); // Some attribute, no idea what for
	p_spb->get_u32(); // Vertex format index

	// 6 unused uints
	p_spb->seek(p_spb->get_position() + 6 * sizeof(uint32_t));

	p_spb->get_data((uint8_t *)p_mesh->name, 64);

	p_mesh->name[sizeof(p_mesh->name) - 1] = 0;

	if (submesh_offset != 0) {
		p_mesh->submeshes.resize(submesh_count);
		for (uint32_t i = 0; i < submesh_count; i++) {
			p_spb->seek(p_base_offset + submesh_offset + sub_mesh_size * i);
			read_submesh(&p_mesh->submeshes[i], p_spb, p_base_offset);
		}
	}

	read_model_vertex_data_godot(p_mesh, p_spb, p_base_offset, vertex_offsets, vertex_count, vertex_format);
	//read_model_vertex_data(p_mesh, p_spb, p_base_offset, vertex_offsets, vertex_count, vertex_format);
}
void DIVAObjectSet::read_model(DIVAObject *p_obj, Ref<StreamPeerBuffer> p_spb, uint32_t p_base_offset) {
	const uint32_t mesh_size = 0xD8;

	p_spb->seek(p_base_offset);

	uint32_t signature = p_spb->get_u32();
	p_spb->get_u32(); // flags

	p_obj->bounding_sphere.center.x = p_spb->get_float();
	p_obj->bounding_sphere.center.y = p_spb->get_float();
	p_obj->bounding_sphere.center.z = p_spb->get_float();
	p_obj->bounding_sphere.radius = p_spb->get_float();

	uint32_t mesh_count = p_spb->get_u32();
	uint32_t meshes_offset = p_spb->get_u32();
	uint32_t material_count = p_spb->get_u32();
	uint32_t materials_offset = p_spb->get_u32();

	// 10 unused values?
	p_spb->seek(p_spb->get_position() + sizeof(uint32_t) * 10);

	p_obj->meshes.resize(mesh_count);

	DIVAReadHelpers::OffsetQueue queue{
		.spb = p_spb
	};

	for (uint32_t i = 0; i < mesh_count; i++) {
		queue.position_push(p_base_offset + meshes_offset + mesh_size * i);

		read_mesh(&p_obj->meshes[i], p_spb, p_base_offset);

		queue.position_pop();
	}
};

void DIVAObjectSet::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_object_meshes", "object_name"), &DIVAObjectSet::get_object_meshes_bind);
	ClassDB::bind_method(D_METHOD("read_classic", "spb"), &DIVAObjectSet::read_classic);
}

void DIVAObjectSet::read_classic(Ref<StreamPeerBuffer> p_spb) {
	uint32_t version = p_spb->get_u32();

	if (version != 0x05062500) {
		return;
	}

	uint32_t object_count = p_spb->get_u32();

	ObjectSetHeader header = {
		.last_obj_id = p_spb->get_u32(),
		.obj_datas_offset = p_spb->get_u32(),
		.obj_skins_offset = p_spb->get_u32(),
		.obj_names_offset = p_spb->get_u32(),
		.obj_ids_offset = p_spb->get_u32(),
		.tex_ids_offset = p_spb->get_u32(),
	};

	uint32_t tex_id_count = p_spb->get_u32();
	// ??
	p_spb->seek(p_spb->get_position() + 8);

	objects.resize(object_count);

	DIVAReadHelpers::OffsetQueue queue{
		.spb = p_spb
	};

	queue.position_push(header.obj_datas_offset);

	for (uint32_t i = 0; i < object_count; i++) {
		read_model(&objects[i], p_spb, p_spb->get_u32());
	}

	p_spb->seek(header.obj_names_offset);
	for (uint32_t i = 0; i < object_count; i++) {
		objects[i].name = DIVAReadHelpers::read_null_terminated_string(p_spb->get_u32(), queue);
		name_to_object_map.insert(StringName(objects[i].name), i);
	}

	queue.position_pop();
}
