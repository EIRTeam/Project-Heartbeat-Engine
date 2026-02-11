#include "../diva/bone_db.h"
#include "../diva/item_table.h"
#include "../diva/motion.h"
#include "../diva/motion_db.h"
#include "../diva/object_db.h"
#include "../diva/sprite_db.h"
#include "modules/hbnative/diva/diva_object.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/resources/packed_scene.h"
#include "servers/rendering/rendering_server_default.h"
#include "tests/test_macros.h"

namespace TestDIVAMOT {
static const int TEST_APPID = 1216230;
TEST_SUITE("[DIVAMotion]") {
	TEST_CASE("[DIVAMotion] motdb loading") {
		DIVAMotionDB diva_mot_db = DIVAMotionDB();
		Ref<FileAccess> test_file = FileAccess::open("/home/eirexe/.local/share/Project Heartbeat/mot_db.bin", FileAccess::READ);
		Ref<StreamPeerBuffer> spb;
		spb.instantiate();
		spb->set_data_array(test_file->get_buffer(test_file->get_length()));
		diva_mot_db.read(spb);
		diva_mot_db.dump_json("/home/eirexe/.local/share/Project Heartbeat/motdb_dump.json");
	}
	TEST_CASE("[DIVAMotion] bonedb loading") {
		DIVABoneDB diva_bone_db = DIVABoneDB();
		Ref<FileAccess> test_file = FileAccess::open("/home/eirexe/.local/share/Project Heartbeat/bone_data.bin", FileAccess::READ);
		Ref<StreamPeerBuffer> spb;
		spb.instantiate();
		spb->set_data_array(test_file->get_buffer(test_file->get_length()));
		diva_bone_db.read_classic(spb);
		diva_bone_db.dump_json("/home/eirexe/.local/share/Project Heartbeat/bonedb_dump.json");
	}

	TEST_CASE("[DIVAMotion] spritedb loading") {
		DIVASpriteDB sprite_db = DIVASpriteDB();
		Ref<FileAccess> test_file = FileAccess::open("/home/eirexe/.local/share/Project Heartbeat/spr_db.bin", FileAccess::READ);
		Ref<StreamPeerBuffer> spb;
		spb.instantiate();
		spb->set_data_array(test_file->get_buffer(test_file->get_length()));
		sprite_db.read_classic(spb);
		sprite_db.dump_json("/home/eirexe/.local/share/Project Heartbeat/spritedb_dump.json");
	}

	TEST_CASE("[DIVAMotion] item table loading") {
		Ref<DIVAItemTable> item_table;
		item_table.instantiate();

		Ref<FileAccess> test_file = FileAccess::open("/home/eirexe/.local/share/Project Heartbeat/mikitm_tbl.txt", FileAccess::READ);
		item_table->parse(test_file->get_as_utf8_string());
	}
	TEST_CASE("[DIVAMotion] objectdb loading") {
		Ref<DIVAObjectDB> object_db;
		object_db.instantiate();
		Ref<FileAccess> test_file = FileAccess::open("/home/eirexe/.local/share/Project Heartbeat/obj_db.bin", FileAccess::READ);
		Ref<StreamPeerBuffer> spb;
		spb.instantiate();
		spb->set_data_array(test_file->get_buffer(test_file->get_length()));
		object_db->read_classic(spb);
		object_db->dump_json("/home/eirexe/.local/share/Project Heartbeat/objdb_dump.json");
	}

	TEST_CASE("[DIVAMotion][SceneTree] object set loading") {
		Ref<DIVAObjectSet> object_set;
		object_set.instantiate();
		Ref<FileAccess> test_file = FileAccess::open("/home/eirexe/.local/share/Project Heartbeat/mikitm625_obj.bin", FileAccess::READ);
		Ref<StreamPeerBuffer> spb;
		spb.instantiate();
		spb->set_data_array(test_file->get_buffer(test_file->get_length()));
		object_set->read_classic(spb);

		Vector<Ref<Mesh>> meshes = object_set->get_object_meshes(StringName("mikitm625_atam_atama_125__divskn"));

		Node3D *root = memnew(Node3D);
		root->set_name("root");

		for (int i = 0; i < meshes.size(); i++) {
			MeshInstance3D *mi = memnew(MeshInstance3D);
			mi->set_mesh(meshes[i]);
			print_line(meshes[i]->get_surface_count());
			root->add_child(mi, true);
		}
		Ref<PackedScene> ps;
		ps.instantiate();
		ps->pack(root);
		ResourceSaver::save(ps, "/home/eirexe/.local/share/Project Heartbeat/cock.tscn");
		root->queue_free();
	}

	TEST_CASE("[DIVAMotion] spriteset loading") {
		Ref<DIVASpriteSet> sprite_set;
		sprite_set.instantiate();
		Ref<FileAccess> test_file = FileAccess::open("/home/eirexe/.local/share/Project Heartbeat/spr_sel_md528cmn.bin", FileAccess::READ);
		Ref<StreamPeerBuffer> spb;
		spb.instantiate();
		spb->set_data_array(test_file->get_buffer(test_file->get_length()));
		sprite_set->read_classic(spb);
		sprite_set->get_texture_set()->dump_json("/home/eirexe/.local/share/Project Heartbeat/textureset_dump.json");
		Vector<Ref<Image>> mipmaps = sprite_set->get_texture_set()->get_texture_mipmaps(0);
		for (int i = 0; i < mipmaps.size(); i++) {
			Ref<Image> img = mipmaps[i];
			img->decompress();
			img->convert(Image::FORMAT_RGBA8);
		}

		// Now, convert this to RGB
		Projection matrix;
		matrix.columns[0] = Vector4(1.0000, 1.0000, 1.0000, 0.0000);
		matrix.columns[1] = Vector4(0.0000, -0.3441, 1.7720, 0.0000);
		matrix.columns[2] = Vector4(1.4020, -0.7141, 0.0000, 0.0000);
		matrix.columns[3] = Vector4(-0.7010, 0.5291, -0.8860, 1.0000);

		for (int x = 0; x < mipmaps[0]->get_size().x; x++) {
			for (int y = 0; y < mipmaps[0]->get_size().y; y++) {
				Size2i pixel_point = Size2i(x, y);
				Color y_a = mipmaps[0]->get_pixelv(pixel_point);
				Color cb_cr = mipmaps[1]->get_pixelv(pixel_point / 2);
				Vector4 out_color = Vector4(y_a.r, cb_cr.r, cb_cr.g, 1.0f);
				out_color = matrix.xform(out_color);
				Color out_col = Color(out_color.x, out_color.y, out_color.z, y_a.g);
				mipmaps[0]->set_pixelv(pixel_point, out_col);
			}
		}

		mipmaps[0]->flip_y();
		mipmaps[0]->save_png("/home/eirexe/.local/share/Project Heartbeat/sprite_dump.png");
	}

	TEST_CASE("[DIVAMotion] moduledb loading") {
		DIVASpriteDB sprite_db = DIVASpriteDB();
		Ref<FileAccess> test_file = FileAccess::open("/home/eirexe/.local/share/Project Heartbeat/spr_db.bin", FileAccess::READ);
		Ref<StreamPeerBuffer> spb;
		spb.instantiate();
		spb->set_data_array(test_file->get_buffer(test_file->get_length()));
		sprite_db.read_classic(spb);
		sprite_db.dump_json("/home/eirexe/.local/share/Project Heartbeat/spritedb_dump.json");
	}
}
} //namespace TestDIVAMOT