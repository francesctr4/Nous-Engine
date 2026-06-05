#include "Engine/Systems/VideoSystem/VideoProbe.h"

#include "Engine/Core/Logger/Logger.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}

bool ProbeVideoFile(const std::string& libraryPath, VideoProbeInfo& outInfo)
{
    AVFormatContext* fmt = nullptr;

    if (avformat_open_input(&fmt, libraryPath.c_str(), nullptr, nullptr) != 0)
    {
        NOUS_WARN("ProbeVideoFile: avformat_open_input failed for '%s'", libraryPath.c_str());
        return false;
    }

    if (avformat_find_stream_info(fmt, nullptr) < 0)
    {
        NOUS_WARN("ProbeVideoFile: avformat_find_stream_info failed for '%s'", libraryPath.c_str());
        avformat_close_input(&fmt);
        return false;
    }

    const int videoStreamIndex = av_find_best_stream(fmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (videoStreamIndex < 0)
    {
        NOUS_WARN("ProbeVideoFile: no video stream in '%s'", libraryPath.c_str());
        avformat_close_input(&fmt);
        return false;
    }

    const AVStream*          videoStream = fmt->streams[videoStreamIndex];
    const AVCodecParameters* codecPar    = videoStream->codecpar;

    outInfo.width  = static_cast<uint32>(codecPar->width);
    outInfo.height = static_cast<uint32>(codecPar->height);

    if (const AVCodecDescriptor* desc = avcodec_descriptor_get(codecPar->codec_id))
        outInfo.codecName = desc->name;
    else
        outInfo.codecName = "unknown";

    outInfo.durationSec = (fmt->duration != AV_NOPTS_VALUE)
        ? static_cast<float>(fmt->duration) / static_cast<float>(AV_TIME_BASE)
        : 0.0f;

    const AVRational fr = videoStream->avg_frame_rate;
    outInfo.frameRate = (fr.num != 0 && fr.den != 0)
        ? static_cast<float>(fr.num) / static_cast<float>(fr.den)
        : 0.0f;

    outInfo.hasAudioTrack =
        av_find_best_stream(fmt, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0) >= 0;

    avformat_close_input(&fmt);
    return true;
}
