#include "interval_tree.h"

void HBIntervalTree::_bind_methods() {
	ClassDB::bind_method(D_METHOD("insert", "low", "high", "value"), &HBIntervalTree::insert);
	ClassDB::bind_method(D_METHOD("erase", "low", "high", "value"), &HBIntervalTree::erase);
	ClassDB::bind_method(D_METHOD("query_point", "query_point"), &HBIntervalTree::query_point);
	ClassDB::bind_method(D_METHOD("clear"), &HBIntervalTree::clear);
}

void HBIntervalTree::insert(int64_t p_low, int64_t p_high, ObjectID p_value) {
	IntervalTree::Interval interval{ p_low, p_high, p_value };
	ERR_FAIL_COND(!tree.insert(interval));
}

void HBIntervalTree::erase(int64_t p_low, int64_t p_high, ObjectID p_value) {
	IntervalTree::Interval interval{ p_low, p_high, p_value };
	ERR_FAIL_COND(!tree.remove(interval));
}

TypedArray<Object> HBIntervalTree::query_point(int64_t p_point) const {
	IntervalTree::Intervals intervals = tree.findIntervalsContainPoint(p_point);
	TypedArray<Object> intervals_out;
	intervals_out.resize(intervals.size());

	for (int i = 0; i < (int)intervals.size(); i++) {
		intervals_out[i] = ObjectDB::get_instance(intervals[i].value);
	}
	return intervals_out;
}

void HBIntervalTree::clear() {
	tree.clear();
}
