#ifndef RECTPACK_H
#define RECTPACK_H

#include "core/math/rect2.h"
#include "core/object/class_db.h"
#include "core/variant/dictionary.h"
#include "core/variant/typed_array.h"
#include "thirdparty/misc/stb_rect_pack.h"

class HBRectPack : public Object {
	GDCLASS(HBRectPack, Object);
	static constexpr int MAX_RECT_SIZE = 8192;
	static stbrp_node packer_temp_storage[MAX_RECT_SIZE];

protected:
	static void _bind_methods();

public:
	static Dictionary pack_rects(PackedVector2Array p_rects, Vector2i p_starting_pack_size);
};

#endif // RECTPACK_H
