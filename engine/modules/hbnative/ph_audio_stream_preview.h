/*************************************************************************/
/*  audio_stream_preview.h                                               */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2021 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2021 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#ifndef PH_AUDIO_STREAM_PREVIEW_H
#define PH_AUDIO_STREAM_PREVIEW_H

#include "core/object/ref_counted.h"
#include "core/os/thread.h"
#include "scene/main/node.h"
#include "servers/audio/audio_stream.h"

class PHAudioStreamPreview : public RefCounted {
	GDCLASS(PHAudioStreamPreview, RefCounted);
	friend class AudioStream;
	Vector<uint8_t> preview;
	float length;

	friend class PHAudioStreamPreviewGenerator;

protected:
	static void _bind_methods();

public:
	float get_length() const;
	float get_max(float p_time, float p_time_next) const;
	float get_min(float p_time, float p_time_next) const;
	float get_avg(float p_time, float p_time_next) const;
	float get_rms(float p_time, float p_time_next) const;

	PHAudioStreamPreview();
};

class PHAudioStreamPreviewGenerator : public Node {
	GDCLASS(PHAudioStreamPreviewGenerator, Node);

	static PHAudioStreamPreviewGenerator *singleton;

	struct Preview {
		Ref<PHAudioStreamPreview> preview;
		Ref<AudioStream> base_stream;
		Ref<AudioStreamPlayback> playback;
		SafeFlag generating;
		ObjectID id;
		Thread *thread;

		// Needed for the bookkeeping of the Map
		Preview &operator=(const Preview &p_rhs) {
			preview = p_rhs.preview;
			base_stream = p_rhs.base_stream;
			playback = p_rhs.playback;
			generating.set_to(generating.is_set());
			id = p_rhs.id;
			thread = p_rhs.thread;
			return *this;
		}
		Preview(const Preview &p_rhs) {
			preview = p_rhs.preview;
			base_stream = p_rhs.base_stream;
			playback = p_rhs.playback;
			generating.set_to(generating.is_set());
			id = p_rhs.id;
			thread = p_rhs.thread;
		}
		Preview() {
		}
	};

	RBMap<ObjectID, Preview> previews;

	static void _preview_thread(void *p_preview);

	void _update_emit(ObjectID p_id);

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	static PHAudioStreamPreviewGenerator *get_singleton() { return singleton; }

	Ref<PHAudioStreamPreview> generate_preview(const Ref<AudioStream> &p_stream);

	PHAudioStreamPreviewGenerator();
};

#endif // AUDIO_STREAM_PREVIEW_H
