#include "rectpack.h"
#include "core/error/error_list.h"
#include "core/math/rect2i.h"
#include "core/object/class_db.h"
#include "core/typedefs.h"
#include "core/variant/dictionary.h"
#include "core/variant/typed_array.h"

stbrp_node HBRectPack::packer_temp_storage[MAX_RECT_SIZE] = {};

void HBRectPack::_bind_methods() {
	ClassDB::bind_static_method("HBRectPack", D_METHOD("pack_rects", "rects", "starting_pack_size"), &HBRectPack::pack_rects);
}

Dictionary HBRectPack::pack_rects(PackedVector2Array p_rects, Vector2i p_starting_pack_size) {
	LocalVector<stbrp_rect> rects;
	rects.resize(p_rects.size());
	for (int i = 0; i < p_rects.size(); i++) {
		rects[i].id = i;
		Vector2i rect = p_rects[i];
		rects[i].w = rect.x;
		rects[i].h = rect.y;
	}

	Vector2i pack_size = Vector2(
			nearest_power_of_2_templated(p_starting_pack_size.x),
			nearest_power_of_2_templated(p_starting_pack_size.y));

	bool packed = false;

	stbrp_context context;

	while (!packed && pack_size != Vector2i(MAX_RECT_SIZE, MAX_RECT_SIZE)) {
		int min_axis = pack_size.min_axis_index();
		pack_size.coord[min_axis] <<= 1;

		stbrp_init_target(&context, pack_size.x, pack_size.y, packer_temp_storage, MAX_RECT_SIZE);
		int result = stbrp_pack_rects(&context, rects.ptr(), rects.size());
		if (result == 1) {
			packed = true;
			break;
		}
	}

	Dictionary out;

	if (!packed) {
		out["result"] = FAILED;
		return out;
	}

	TypedArray<Vector2i> out_points;
	out_points.resize(rects.size());

	for (size_t i = 0; i < rects.size(); i++) {
		const Vector2i pos = Vector2i(rects[i].x, rects[i].y);
		out_points[i] = pos;
	}

	out["size"] = pack_size;
	out["result"] = OK;
	out["points"] = out_points;

	return out;
}
