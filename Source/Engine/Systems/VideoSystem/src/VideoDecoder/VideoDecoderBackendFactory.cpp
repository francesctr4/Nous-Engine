#include <VideoSystem/VideoDecoder/VideoDecoderBackendFactory.h>

#include "VideoDecoder/Backends/ffmpeg/FFmpegBackend.h"
#include <MemoryManager/MemoryManager.h>
#include <Logger/LogChannel.h>
#include <Logger/Logger.h>

constexpr LogChannel CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_SYSTEM_VIDEOSYSTEM;

IVideoDecoderBackend* CreateVideoDecoderBackend(const VideoDecoderBackend type)
{
    IVideoDecoderBackend* backend = nullptr;

    switch (type)
    {
        case VideoDecoderBackend::FFMPEG:
            backend = NOUS_NEW<FFmpegBackend>(MemoryTag::VIDEO_SYSTEM);
            break;
        case VideoDecoderBackend::UNKNOWN:
        default:
            NOUS_ERROR_C(CURRENT_CHANNEL, "Unknown video backend type: %d", static_cast<int>(type));
            return nullptr;
    }

    if (!backend)
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "Failed to create video backend type (%d).", static_cast<int>(type));
        return nullptr;
    }

    NOUS_INFO_C(CURRENT_CHANNEL, "Created video backend type (%d) successfully.", static_cast<int>(type));
    return backend;
}
