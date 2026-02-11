#ifndef INTERVAL_TREE_GD_H
#define INTERVAL_TREE_GD_H

#include "core/object/ref_counted.h"
#include "core/variant/typed_array.h"
#include "thirdparty/intervaltree.h"

class HBIntervalTree : public RefCounted {
	GDCLASS(HBIntervalTree, RefCounted);

	typedef Intervals::IntervalTree<int64_t, ObjectID> IntervalTree;
	IntervalTree tree;

protected:
	static void _bind_methods();

public:
	void insert(int64_t p_low, int64_t p_high, ObjectID p_value);
	void erase(int64_t p_low, int64_t p_high, ObjectID p_value);
	TypedArray<Object> query_point(int64_t p_point) const;
	void clear();
};

#endif // INTERVAL_TREE_GD_H
