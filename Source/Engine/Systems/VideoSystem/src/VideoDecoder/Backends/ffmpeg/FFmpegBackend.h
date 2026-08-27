#pragma once

#include <VideoSystem/VideoDecoder/IVideoDecoderBackend.h>

struct FFmpegBackend : IVideoDecoderBackend
{
    bool        Initialize() override;
    void        Shutdown() noexcept override;

    VideoHandle CreateVideo(ResourceVideo* rVideo) override;
    void        DestroyVideo(VideoHandle handle) noexcept override;

    void        Start(VideoHandle handle) override;
    void        Stop(VideoHandle handle) override;
    void        SetLooping(VideoHandle handle, bool looping) override;

    bool        TryGetFrame(VideoHandle handle, double playheadSec, VideoFrame& out) override;

    void        GetDimensions(VideoHandle handle, uint32& width, uint32& height) const override;
    float       GetFrameRate(VideoHandle handle) const override;
    float       GetDuration(VideoHandle handle) const override;
    bool        IsFinished(VideoHandle handle) const override;
};
