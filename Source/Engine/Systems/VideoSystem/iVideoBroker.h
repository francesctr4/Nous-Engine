#pragma once

#include "Engine/Systems/VideoSystem/VideoHandle.h"
#include "Engine/Systems/VideoSystem/VideoFrame.h"

class ResourceVideo;

// -----------------------------------------------------------------------------
// The video decoder, seen from inside Systems/.
// -----------------------------------------------------------------------------
/**
 * @brief Decoder handle surface, as needed by CVideoPlayer.
 *
 * Implemented by ModuleVideo so CVideoPlayer can drive a decoder without
 * depending on Modules/. Mirrors the existing IResourceLoader seam.
 *
 * Covers only what CVideoPlayer uses: Stop / GetDimensions / GetFrameRate /
 * IsFinished have no consumer in Systems/ and stay off the interface.
 */
class IVideoBroker
{
public:
    virtual ~IVideoBroker() = default;

    virtual VideoHandle CreateVideo(ResourceVideo* rVideo) const = 0;
    virtual void        DestroyVideo(VideoHandle handle) const noexcept = 0;

    virtual void        Start(VideoHandle handle) const = 0;
    virtual void        SetLooping(VideoHandle handle, bool looping) const = 0;

    virtual float       GetDuration(VideoHandle handle) const = 0;
    virtual bool        TryGetFrame(VideoHandle handle, double playheadSec, VideoFrame& out) const = 0;
};
