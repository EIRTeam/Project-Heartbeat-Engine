#ifndef READ_HELPERS_H
#define READ_HELPERS_H

#include "core/io/stream_peer.h"
#include <stack>

namespace DIVAReadHelpers {
struct OffsetQueue {
	Ref<StreamPeerBuffer> spb;
	std::stack<int> position_queue;
	void position_push(int p_pos) {
		position_queue.push(spb->get_position());
		spb->seek(p_pos);
	}
	void position_pop() {
		int pos = position_queue.top();
		position_queue.pop();
		spb->seek(pos);
	}
};

static String read_null_terminated_string(int p_string_pos, OffsetQueue &p_queue) {
	int length = 0;
	uint8_t chr;

	p_queue.position_push(p_string_pos);
	while (p_queue.spb->get_available_bytes() > 0 && (chr = p_queue.spb->get_u8()) != 0) {
		length++;
	}
	p_queue.position_pop();

	p_queue.position_push(p_string_pos);
	String out = p_queue.spb->get_string(length);
	p_queue.position_pop();
	return out;
}
}; //namespace DIVAReadHelpers

#endif // READ_HELPERS_H
