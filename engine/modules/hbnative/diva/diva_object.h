#ifndef DIVA_OBJECT_H
#define DIVA_OBJECT_H

#include "core/io/stream_peer.h"
#include "core/object/ref_counted.h"
#include "read_helpers.h"
#include "scene/resources/mesh.h"

class DIVAObjectSet : public RefCounted {
	GDCLASS(DIVAObjectSet, RefCounted);

	struct DIVAVertexData {
		Vector3 position;
		Vector3 normal;
		Vector4 tangent;
		Vector3 binormal;
		Vector2 texcoord0;
		Vector2 texcoord1;
		Vector2 texcoord2;
		Vector2 texcoord3;
		Color color0;
		Color color1;
		float bone_weights[4];
		int16_t bone_indices[4];
	};

	struct DIVABoundingBox {
		Vector3 center;
		Vector3 size;
	};

	struct DIVABoundingSphere {
		Vector3 center;
		float radius;
	};

	enum DIVAIndexFormat : uint32_t {
		OBJ_INDEX_U8 = 0x00,
		OBJ_INDEX_U16 = 0x01,
		OBJ_INDEX_U32 = 0x02,
	};

	enum DIVAPrimitive : uint32_t {
		OBJ_PRIMITIVE_POINTS = 0x00,
		OBJ_PRIMITIVE_LINES = 0x01,
		OBJ_PRIMITIVE_LINE_STRIP = 0x02,
		OBJ_PRIMITIVE_LINE_LOOP = 0x03,
		OBJ_PRIMITIVE_TRIANGLES = 0x04,
		OBJ_PRIMITIVE_TRIANGLE_STRIP = 0x05,
		OBJ_PRIMITIVE_TRIANGLE_FAN = 0x06,
		OBJ_PRIMITIVE_QUADS = 0x07,
		OBJ_PRIMITIVE_QUAD_STRIP = 0x08,
		OBJ_PRIMITIVE_POLYGON = 0x09,
	};

	enum DIVAVertexFormat {
		OBJ_VERTEX_FILE_POSITION = 0x00000001,
		OBJ_VERTEX_FILE_NORMAL = 0x00000002,
		OBJ_VERTEX_FILE_TANGENT = 0x00000004,
		OBJ_VERTEX_FILE_BINORMAL = 0x00000008,
		OBJ_VERTEX_FILE_TEXCOORD0 = 0x00000010,
		OBJ_VERTEX_FILE_TEXCOORD1 = 0x00000020,
		OBJ_VERTEX_FILE_TEXCOORD2 = 0x00000040,
		OBJ_VERTEX_FILE_TEXCOORD3 = 0x00000080,
		OBJ_VERTEX_FILE_COLOR0 = 0x00000100,
		OBJ_VERTEX_FILE_COLOR1 = 0x00000200,
		OBJ_VERTEX_FILE_BONE_WEIGHT = 0x00000400,
		OBJ_VERTEX_FILE_BONE_INDEX = 0x00000800,
		OBJ_VERTEX_FILE_UNKNOWN = 0x00001000,
		OBJ_VERTEX_FILE_MODERN_STORAGE = 0x80000000,
	};

	struct ObjectSetHeader {
		uint32_t last_obj_id;
		uint32_t obj_datas_offset;
		uint32_t obj_skins_offset;
		uint32_t obj_names_offset;
		uint32_t obj_ids_offset;
		uint32_t tex_ids_offset;
	};

	struct DIVASubmesh {
		uint32_t flags;
		DIVABoundingSphere bounding_sphere;
		DIVABoundingBox bounding_box;
		DIVAPrimitive primitive;
		DIVAIndexFormat index_format;
		uint32_t bones_per_vertex;
		uint32_t material;
		uint8_t uv_indices[8];
		LocalVector<uint16_t> bone_index_array;
		LocalVector<int32_t> index_array;
	};

	struct DIVAMesh {
		uint32_t flags;

		char name[64];
		LocalVector<DIVASubmesh> submeshes;
		bool uses_godot_vertex_data = false;
		LocalVector<DIVAVertexData> vertices;

		struct {
			Vector<Vector3> positions;
			Vector<Vector3> normals;
			Vector<float> tangents;
			Vector<Vector2> texcoord0;
			Vector<Vector2> texcoord1;
			Vector<Color> color0;
			Vector<Color> color1;
			Vector<float> bone_weights;
			Vector<int> bone_indices;
			BitField<ArrayMesh::ArrayFormat> format;
		} godot_vertex_data;

		DIVABoundingSphere bounding_sphere;
	};

	struct DIVAObject {
		DIVABoundingSphere bounding_sphere;
		LocalVector<DIVAMesh> meshes;
		String name;
	};

	LocalVector<DIVAObject> objects;
	HashMap<StringName, int> name_to_object_map;

	static void read_submesh_indices(DIVASubmesh *p_submesh, uint32_t p_index_count, Ref<StreamPeerBuffer> p_spb);
	static void read_submesh(DIVASubmesh *p_submesh, Ref<StreamPeerBuffer> p_spb, uint32_t p_base_offset);
	static void read_model_vertex_data(DIVAMesh *p_mesh, Ref<StreamPeerBuffer> p_spb, uint32_t p_base_offset, uint32_t p_vertex_offsets[20], uint32_t p_vertex_count, uint32_t p_vertex_format);
	static void read_model_vertex_data_godot(DIVAMesh *p_mesh, Ref<StreamPeerBuffer> p_spb, uint32_t p_base_offset, uint32_t p_vertex_offsets[20], uint32_t p_vertex_count, uint32_t p_vertex_format);
	static void read_mesh(DIVAMesh *p_mesh, Ref<StreamPeerBuffer> p_spb, uint32_t p_base_offset);
	static void read_model(DIVAObject *p_obj, Ref<StreamPeerBuffer> p_spb, uint32_t p_base_offset);

protected:
	static void _bind_methods();

public:
	void read_classic(Ref<StreamPeerBuffer> p_spb);
	const DIVAObject *get_object(const StringName &p_object_name) const {
		HashMap<StringName, int>::ConstIterator it = name_to_object_map.find(p_object_name);
		if (it == name_to_object_map.end()) {
			return nullptr;
		}
		return &objects[it->value];
	}

	static Mesh::PrimitiveType diva_primitive_to_godot(DIVAPrimitive p_diva_prim) {
		switch (p_diva_prim) {
			case OBJ_PRIMITIVE_POINTS: {
				return Mesh::PRIMITIVE_POINTS;
			} break;
			case OBJ_PRIMITIVE_LINES: {
				return Mesh::PRIMITIVE_LINES;
			}
			case OBJ_PRIMITIVE_LINE_STRIP: {
				return Mesh::PRIMITIVE_LINE_STRIP;
			}
			case OBJ_PRIMITIVE_LINE_LOOP: {
				// not implemented
				return Mesh::PRIMITIVE_MAX;
			}
			case OBJ_PRIMITIVE_TRIANGLES: {
				return Mesh::PRIMITIVE_TRIANGLES;
			}
			case OBJ_PRIMITIVE_TRIANGLE_STRIP: {
				return Mesh::PRIMITIVE_TRIANGLE_STRIP;
			}
			case OBJ_PRIMITIVE_TRIANGLE_FAN:
			case OBJ_PRIMITIVE_QUADS:
			case OBJ_PRIMITIVE_QUAD_STRIP:
			case OBJ_PRIMITIVE_POLYGON: {
				return Mesh::PRIMITIVE_MAX;
			}
		}
		return Mesh::PRIMITIVE_MAX;
	}

	TypedArray<Mesh> get_object_meshes_bind(const StringName &p_object_name) const {
		Vector<Ref<Mesh>> meshes = get_object_meshes(p_object_name);
		TypedArray<Mesh> meshes_out;
		for (int i = 0; i < meshes.size(); i++) {
			meshes_out.push_back(meshes[i]);
		}
		return meshes_out;
	}
	Vector<Ref<Mesh>> get_object_meshes(const StringName &p_object_name) const {
		const DIVAObject *object = get_object(p_object_name);
		Vector<Ref<Mesh>> meshes;
		for (const DIVAMesh &mesh : object->meshes) {
			Array mesh_data;
			mesh_data.resize(ArrayMesh::ARRAY_MAX);
			Ref<ArrayMesh> am;
			am.instantiate();
			if (mesh.godot_vertex_data.format.has_flag(Mesh::ARRAY_FORMAT_VERTEX)) {
				mesh_data[ArrayMesh::ARRAY_VERTEX] = mesh.godot_vertex_data.positions;
			}
			if (mesh.godot_vertex_data.format.has_flag(Mesh::ARRAY_FORMAT_NORMAL)) {
				mesh_data[ArrayMesh::ARRAY_NORMAL] = mesh.godot_vertex_data.normals;
			}
			if (mesh.godot_vertex_data.format.has_flag(Mesh::ARRAY_FORMAT_TANGENT)) {
				mesh_data[ArrayMesh::ARRAY_TANGENT] = mesh.godot_vertex_data.tangents;
			}
			if (mesh.godot_vertex_data.format.has_flag(Mesh::ARRAY_FORMAT_TEX_UV)) {
				mesh_data[ArrayMesh::ARRAY_TEX_UV] = mesh.godot_vertex_data.texcoord0;
			}
			if (mesh.godot_vertex_data.format.has_flag(Mesh::ARRAY_FORMAT_TEX_UV2)) {
				mesh_data[ArrayMesh::ARRAY_TEX_UV2] = mesh.godot_vertex_data.texcoord1;
			}
			if (mesh.godot_vertex_data.format.has_flag(Mesh::ARRAY_FORMAT_COLOR)) {
				mesh_data[ArrayMesh::ARRAY_COLOR] = mesh.godot_vertex_data.color0;
			}
			if (mesh.godot_vertex_data.format.has_flag(Mesh::ARRAY_FORMAT_WEIGHTS)) {
				mesh_data[ArrayMesh::ARRAY_WEIGHTS] = mesh.godot_vertex_data.bone_weights;
			}
			if (mesh.godot_vertex_data.format.has_flag(Mesh::ARRAY_FORMAT_BONES)) {
				mesh_data[ArrayMesh::ARRAY_BONES] = mesh.godot_vertex_data.bone_indices;
			}

			for (const DIVASubmesh &submesh : mesh.submeshes) {
				PackedInt32Array lv = submesh.index_array;
				mesh_data[ArrayMesh::ARRAY_INDEX] = lv;
				am->add_surface_from_arrays(diva_primitive_to_godot(submesh.primitive), mesh_data, TypedArray<Array>(), Dictionary(), mesh.godot_vertex_data.format);
			}
			meshes.push_back(am);
		}
		return meshes;
	};
};

#endif // DIVA_OBJECT_H
