#include "ph_zip_video_stream.h"

void PHZipFFmpegVideoStream::_bind_methods() {
	ClassDB::bind_static_method("PHZipFFmpegVideoStream", D_METHOD("create", "archive", "path"), &PHZipFFmpegVideoStream::create);
}

Ref<VideoStreamPlayback> PHZipFFmpegVideoStream::instantiate_playback() {
	Ref<FileAccess> fa = archive->get_file(file_path);
	if (!fa.is_valid()) {
		return Ref<VideoStreamPlayback>();
	}
	Ref<FFmpegVideoStreamPlayback> pb;
	pb.instantiate();
	if (pb->load(fa) != OK) {
		return nullptr;
	}
	return pb;
}

Ref<PHZipFFmpegVideoStream> PHZipFFmpegVideoStream::create(Ref<PHZipArchive> p_archive, const String &p_path) {
	Ref<PHZipFFmpegVideoStream> stream;
	stream.instantiate();
	stream->archive = p_archive;
	stream->file_path = p_path;
	return stream;
}
