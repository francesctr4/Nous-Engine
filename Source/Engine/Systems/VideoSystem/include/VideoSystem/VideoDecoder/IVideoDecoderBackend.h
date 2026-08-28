#pragma once

#include <EngineCore/Globals.h>
#include <VideoSystem/VideoFrame.h>
#include <VideoSystem/VideoHandle.h>

class ResourceVideo;

// Codec-agnostic decode seam. The only implementation is FFmpegBackend; a future
// hwaccel / Media Foundation / GStreamer backend slots in here. ResourceVideo is
// forward-declared — only the concrete backend's .cpp includes ResourceVideo.h, the way
// IAudioEngineBackend forward-declares ResourceAudio.
class IVideoDecoderBackend
{
public:
    virtual ~IVideoDecoderBackend() noexcept = default;

    virtual bool Initialize() = 0;
    virtual void Shutdown() noexcept = 0;

    // CreateVideo opens the file and prepares decoding from rVideo's library path +
    // decode mode, but does NOT start playback. The handle is owned by the caller and
    // released with DestroyVideo (which stops + joins any decoder thread).
    virtual VideoHandle CreateVideo(ResourceVideo* rVideo) = 0;
    virtual void        DestroyVideo(VideoHandle handle) noexcept = 0;

    virtual void        Start(VideoHandle handle) = 0;
    virtual void        Stop(VideoHandle handle) = 0;
    virtual void        SetLooping(VideoHandle handle, bool looping) = 0;

    // Newest frame with PTS <= playheadSec, dropping staler frames. Returns false when
    // no new frame is available; out.pixels valid until the next call on this handle.
    virtual bool        TryGetFrame(VideoHandle handle, double playheadSec, VideoFrame& out) = 0;

    virtual void        GetDimensions(VideoHandle handle, uint32& width, uint32& height) const = 0;
    virtual float       GetFrameRate(VideoHandle handle) const = 0;
    virtual float       GetDuration(VideoHandle handle) const = 0;
    virtual bool        IsFinished(VideoHandle handle) const = 0;
};
