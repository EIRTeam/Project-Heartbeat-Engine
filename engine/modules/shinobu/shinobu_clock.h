#ifndef SHINOBU_CLOCK_H
#define SHINOBU_CLOCK_H

#include <atomic>
#include <chrono>

#include "core/object/ref_counted.h"
#include "core/templates/safe_refcount.h"

typedef std::chrono::nanoseconds nanoseconds;

class ShinobuClockLocal : public RefCounted {
	SafeNumeric<uint64_t> last_recorded_time;
	SafeNumeric<uint64_t> last_mix_length_nsec;
};

class ShinobuClock : public RefCounted {
	SafeNumeric<uint64_t> last_recorded_time;
	SafeNumeric<uint64_t> last_mix_length_nsec;
	bool use_mix_size_compensation = true;

public:
	ShinobuClock() {
		last_recorded_time.set(0);
		last_mix_length_nsec.set(0);
	}

	int64_t get_current_offset_nsec() {
		std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
		std::chrono::high_resolution_clock::time_point lrt(nanoseconds(last_recorded_time.get()));
		auto diff = std::chrono::duration_cast<nanoseconds>(now - lrt);
		if (use_mix_size_compensation) {
			return diff.count() - nanoseconds(last_mix_length_nsec.get()).count();
		}
		return diff.count();
	}

	void measure(uint64_t p_mix_length_nsec) {
		uint64_t ns_count = std::chrono::time_point_cast<nanoseconds>(std::chrono::high_resolution_clock::now()).time_since_epoch().count();
		last_recorded_time.set(ns_count);
		last_mix_length_nsec.set(p_mix_length_nsec);
	}

	void set_use_mix_size_compensation(bool p_use_mix_size_compensation) {
		print_line("Using mix size compensation:", p_use_mix_size_compensation);
		use_mix_size_compensation = p_use_mix_size_compensation;
	}
};
#endif