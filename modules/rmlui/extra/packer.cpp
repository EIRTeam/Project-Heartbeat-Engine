#include "packer.h"
#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/string/print_string.h"
#include "core/string/ustring.h"
#include "core/math/geometry_2d.h"
#include "rmlui/extra/rmlui_sprite_sheet.h"
RmlUiPacker::RmlUiPacker(const PackerParams &p_params) : params(p_params) {
    
}

Error RmlUiPacker::pack() {
    String input_file_path;
    const String sprite_sheet_out_path = params.sheet->get_target_path().get_basename() + ".png";
    const String rcss_out_path = params.sheet->get_target_path().get_basename() + ".rcss";
    const int MARGIN = params.sheet->get_margin();
 
    // Collect images and sizes
    Vector<Ref<Image>> images;
    Vector<Vector2i> sizes;

    const int texture_count = params.sheet->textures.size();

    sizes.reserve(texture_count);
    images.reserve(texture_count);

    for (int s = 0; s < texture_count; s++) {
        Ref<RmlUiSpriteSheetTexture> sprite = params.sheet->textures[s];
 
        String sprite_file_path = sprite->get_texture();
        Ref<Image> sprite_img = Image::load_from_file(sprite_file_path);
 
        ERR_FAIL_COND_V_MSG(sprite_img.is_null(), ERR_FILE_NOT_FOUND, vformat("Error loading texture file at %s", sprite_file_path));
 
        sizes.push_back(Vector2(sprite_img->get_size()) + Vector2(MARGIN * 2, MARGIN * 2));
        images.push_back(sprite_img);
    }
 
    // Build atlas
    Vector<Vector2i> atlas_points;
    Vector2i atlas_size;
    Geometry2D::make_atlas(sizes, atlas_points, atlas_size);
 
    // Create output image
    Ref<Image> out_image = Image::create_empty(atlas_size.x, atlas_size.y, false, Image::FORMAT_RGBA8);
 
    for (int j = 0; j < atlas_points.size(); j++) {
        Vector2 point = atlas_points[j];
        Ref<Image> img = images[j];
 
        out_image->blit_rect(
            img,
            Rect2i(Vector2i(), img->get_size()),
            Vector2i(point) + Vector2i(MARGIN, MARGIN)
        );
    }
 
    // Save PNG output
    out_image->save_png(sprite_sheet_out_path);
 
    // Build RCSS
    String sprite_sheet_name = params.sheet->get_spritesheet_name();
    String sprite_sheet_fs_path = sprite_sheet_out_path;
 
    PackedStringArray rcss;
    rcss.push_back(vformat("@spritesheet %s", sprite_sheet_name));
    rcss.push_back("{");
    rcss.push_back(vformat("  src: %s;",
        sprite_sheet_out_path));
 
    HashMap<String, Rect2i> sprite_rects;
 
    for (int j = 0; j < texture_count; j++) {
        Ref<Image> img = images[j];
        String sprite_name = params.sheet->textures[j]->get_sprite_name();
 
        Rect2i rect = Rect2i(
            Vector2i(atlas_points[j]) + Vector2i(MARGIN, MARGIN),
            img->get_size()
        );
        sprite_rects[sprite_name] = rect;
 
        rcss.push_back(vformat("  %s: %dpx %dpx %dpx %dpx;",
            sprite_name,
            rect.position.x, rect.position.y,
            rect.size.x, rect.size.y));
    }
 
    // Process generators (gens)
    /*Array gens = json_dict.get("gens", Array());
    for (int j = 0; j < gens.size(); j++) {
        Dictionary gen = gens[j];
        String gen_type = gen.get("type", "");
 
        if (gen_type == "ninepatch") {
            String source = gen.get("source", "");
            String name = gen.get("name", "");
            int margin = gen.get("margin", 0);
 
            Rect2i rect = sprite_rects[source];
            rect = rect.grow(-margin);
 
            rcss.push_back(vformat("  %s: %dpx %dpx %dpx %dpx;",
                name,
                rect.position.x, rect.position.y,
                rect.size.x, rect.size.y));
        }
    }*/
 
    rcss.push_back("}");
 
    // Write RCSS file
    Ref<FileAccess> f = FileAccess::open(rcss_out_path, FileAccess::WRITE);
    if (f.is_valid()) {
        f->store_string(String("\n").join(rcss));
    } else {
        ERR_PRINT(vformat("Failed to open output RCSS file: %s", sprite_sheet_out_path + ".rcss"));
    }

    return OK;
}
