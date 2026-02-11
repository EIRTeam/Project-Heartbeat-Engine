#include "ph_singleton.h"
#include "thirdparty/linalg.h"
#ifdef SDL_ENABLED
#include "drivers/sdl/joypad_sdl.h"
#endif

PHNative *PHNative::singleton = NULL;

Ref<Process> PHNative::create_process(const String &p_path, const Vector<String> &p_arguments, const String &p_working_dir, bool p_open_stdin) {
	return Process::create(p_path, p_arguments, p_working_dir, p_open_stdin);
}

void PHNative::linalg_test() {
}

Ref<AudioStreamOggVorbis> PHNative::load_ogg_from_file(const String &p_path) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	ERR_FAIL_COND_V_MSG(f.is_null(), Ref<AudioStreamOggVorbis>(), "Cannot open file '" + p_path + "'.");

	uint64_t len = f->get_length();

	Vector<uint8_t> file_data;
	file_data.resize(len);
	uint8_t *w = file_data.ptrw();

	f->get_buffer(w, len);
	return load_ogg_from_buffer(file_data);
}

Ref<AudioStreamOggVorbis> PHNative::load_ogg_from_buffer(const Vector<uint8_t> &p_buffer) {
	uint64_t len = p_buffer.size();

	Ref<AudioStreamOggVorbis> ogg_vorbis_stream;
	ogg_vorbis_stream.instantiate();

	Ref<OggPacketSequence> ogg_packet_sequence;
	ogg_packet_sequence.instantiate();

	ogg_stream_state stream_state;
	ogg_sync_state sync_state;
	ogg_page page;
	ogg_packet packet;
	bool initialized_stream = false;

	ogg_sync_init(&sync_state);
	int err;
	size_t cursor = 0;
	size_t packet_count = 0;
	bool done = false;
	while (!done) {
		err = ogg_sync_check(&sync_state);
		ERR_FAIL_COND_V_MSG(err != 0, Ref<AudioStreamOggVorbis>(), "Ogg sync error " + itos(err));
		while (ogg_sync_pageout(&sync_state, &page) != 1) {
			if (cursor >= len) {
				done = true;
				break;
			}
			err = ogg_sync_check(&sync_state);
			ERR_FAIL_COND_V_MSG(err != 0, Ref<AudioStreamOggVorbis>(), "Ogg sync error " + itos(err));
			char *sync_buf = ogg_sync_buffer(&sync_state, OGG_SYNC_BUFFER_SIZE);
			err = ogg_sync_check(&sync_state);
			ERR_FAIL_COND_V_MSG(err != 0, Ref<AudioStreamOggVorbis>(), "Ogg sync error " + itos(err));
			ERR_FAIL_COND_V(cursor > len, Ref<AudioStreamOggVorbis>());
			size_t copy_size = len - cursor;
			if (copy_size > OGG_SYNC_BUFFER_SIZE) {
				copy_size = OGG_SYNC_BUFFER_SIZE;
			}
			memcpy(sync_buf, &p_buffer[cursor], copy_size);
			ogg_sync_wrote(&sync_state, copy_size);
			cursor += copy_size;
			err = ogg_sync_check(&sync_state);
			ERR_FAIL_COND_V_MSG(err != 0, Ref<AudioStreamOggVorbis>(), "Ogg sync error " + itos(err));
		}
		if (done) {
			break;
		}
		err = ogg_sync_check(&sync_state);
		ERR_FAIL_COND_V_MSG(err != 0, Ref<AudioStreamOggVorbis>(), "Ogg sync error " + itos(err));

		// Have a page now.
		if (!initialized_stream) {
			if (ogg_stream_init(&stream_state, ogg_page_serialno(&page))) {
				ERR_FAIL_V_MSG(Ref<AudioStreamOggVorbis>(), "Failed allocating memory for Ogg Vorbis stream.");
			}
			initialized_stream = true;
		}
		ogg_stream_pagein(&stream_state, &page);
		err = ogg_stream_check(&stream_state);
		ERR_FAIL_COND_V_MSG(err != 0, Ref<AudioStreamOggVorbis>(), "Ogg stream error " + itos(err));
		int desync_iters = 0;

		Vector<Vector<uint8_t>> packet_data;
		int64_t granule_pos = 0;

		while (true) {
			err = ogg_stream_packetout(&stream_state, &packet);
			if (err == -1) {
				// According to the docs this is usually recoverable, but don't sit here spinning forever.
				desync_iters++;
				ERR_FAIL_COND_V_MSG(desync_iters > 100, Ref<AudioStreamOggVorbis>(), "Packet sync issue during Ogg import");
				continue;
			} else if (err == 0) {
				// Not enough data to fully reconstruct a packet. Go on to the next page.
				break;
			}
			if (packet_count == 0 && vorbis_synthesis_idheader(&packet) == 0) {
				print_verbose("Found a non-vorbis-header packet in a header position");
				// Clearly this logical stream is not a vorbis stream, so destroy it and try again with the next page.
				if (initialized_stream) {
					ogg_stream_clear(&stream_state);
					initialized_stream = false;
				}
				break;
			}
			granule_pos = packet.granulepos;

			PackedByteArray data;
			data.resize(packet.bytes);
			memcpy(data.ptrw(), packet.packet, packet.bytes);
			packet_data.push_back(data);
			packet_count++;
		}
		if (initialized_stream) {
			ogg_packet_sequence->push_page(granule_pos, packet_data);
		}
	}
	if (initialized_stream) {
		ogg_stream_clear(&stream_state);
	}
	ogg_sync_clear(&sync_state);

	if (ogg_packet_sequence->get_packet_granule_positions().is_empty()) {
		ERR_FAIL_V_MSG(Ref<AudioStreamOggVorbis>(), "Ogg Vorbis decoding failed. Check that your data is a valid Ogg Vorbis audio stream.");
	}

	ogg_vorbis_stream->set_packet_sequence(ogg_packet_sequence);
	ogg_vorbis_stream->set_meta("raw_file_data", p_buffer);

	return ogg_vorbis_stream;
}

String PHNative::get_rendering_api_name() {
	return OS::get_singleton()->get_current_rendering_driver_name();
	RenderingDevice *rd = RenderingDevice::get_singleton();
	if (rd) {
		return rd->get_device_api_name();
	}

	return "Unknown";
}

bool PHNative::is_sdl_device_game_controller(int p_joy_device_idx) {
#ifdef SDL_ENABLED
	if (JoypadSDL *sdl = JoypadSDL::get_singleton(); sdl) {
		return sdl->is_device_game_controller(p_joy_device_idx);
	}
#else
	return false;
#endif
}

String PHNative::get_sdl_device_guid(int p_joy_device_idx) {
#ifdef SDL_ENABLED
	if (JoypadSDL *sdl = JoypadSDL::get_singleton(); sdl) {
		return sdl->get_device_guid(p_joy_device_idx);
	}
#else
	return "";
#endif
}

void PHNative::_bind_methods() {
	ClassDB::bind_method(D_METHOD("create_process", "path", "arguments", "working_directory", "open_stdin"), &PHNative::create_process, DEFVAL(Vector<String>()), DEFVAL(""), DEFVAL(false));
	ClassDB::bind_static_method("PHNative", D_METHOD("load_ogg_from_file", "path"), &PHNative::load_ogg_from_file);
	ClassDB::bind_static_method("PHNative", D_METHOD("load_ogg_from_buffer", "buffer"), &PHNative::load_ogg_from_buffer);
	ClassDB::bind_static_method("PHNative", D_METHOD("get_rendering_api_name"), &PHNative::get_rendering_api_name);
	ClassDB::bind_static_method("PHNative", D_METHOD("is_sdl_device_game_controller"), &PHNative::is_sdl_device_game_controller);
	ClassDB::bind_static_method("PHNative", D_METHOD("get_sdl_device_guid"), &PHNative::get_sdl_device_guid);

	ClassDB::bind_method(D_METHOD("get_blur_controls_enabled"), &PHNative::get_blur_controls_enabled);
	ClassDB::bind_method(D_METHOD("set_blur_controls_enabled", "blur_controls_enabled"), &PHNative::set_blur_controls_enabled);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "blur_controls_enabled"), "set_blur_controls_enabled", "get_blur_controls_enabled");

	ADD_SIGNAL(MethodInfo("blur_controls_enabled_changed"));
}

PHNative::PHNative() {
	singleton = this;
}

bool PHNative::get_blur_controls_enabled() const {
	return blur_controls_enabled;
}
void PHNative::set_blur_controls_enabled(bool p_blur_controls_enabled) {
	blur_controls_enabled = p_blur_controls_enabled;
	emit_signal(SNAME("blur_controls_enabled_changed"));
}
