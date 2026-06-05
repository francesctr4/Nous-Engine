#pragma once

#include "Engine/Core/Globals.h"
#include "Engine/Systems/VideoSystem/VideoFrame.h"
#include "Engine/Systems/VideoSystem/VideoHandle.h"

#include <cstdint>

enum class VideoDecoderBackend : std::int8_t
{
    UNKNOWN = -1,
    FFMPEG  = 0
};

class ResourceVideo;
class IVideoDecoderBackend;

// Thin pass-through over the active decode backend (mirrors AudioSystem). Every call
// safely no-ops / returns null/false when no backend is initialized.
class VideoSystem
{
public:
    VideoSystem();

    bool Initialize(VideoDecoderBackend backend);
    void Shutdown();

    VideoHandle CreateVideo(ResourceVideo* rVideo) const;
    void        DestroyVideo(VideoHandle handle) const noexcept;

    void        Start(VideoHandle handle) const;
    void        Stop(VideoHandle handle) const;
    void        SetLooping(VideoHandle handle, bool looping) const;

    bool        TryGetFrame(VideoHandle handle, double playheadSec, VideoFrame& out) const;

    void        GetDimensions(VideoHandle handle, uint32& width, uint32& height) const;
    float       GetFrameRate(VideoHandle handle) const;
    float       GetDuration(VideoHandle handle) const;
    bool        IsFinished(VideoHandle handle) const;

private:
    IVideoDecoderBackend* m_backend;
};
