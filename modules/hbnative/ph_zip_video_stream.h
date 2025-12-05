#ifndef PH_ZIP_VIDEO_STREAM_H
#define PH_ZIP_VIDEO_STREAM_H

#include "ffmpeg/ffmpeg_video_stream.h"
#include "ph_zip.h"
#include "scene/resources/video_stream.h"

class PHZipFFmpegVideoStream : public VideoStream {
	GDCLASS(PHZipFFmpegVideoStream, VideoStream);

protected:
	static void _bind_methods();
	Ref<VideoStreamPlayback> instantiate_playback() override;

	String file_path;
	Ref<PHZipArchive> archive;

public:
	static Ref<PHZipFFmpegVideoStream> create(Ref<PHZipArchive> p_archive, const String &p_path);
};
#endif // PH_ZIP_VIDEO_STREAM_H
