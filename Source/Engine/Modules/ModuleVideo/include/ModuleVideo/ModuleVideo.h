#pragma once

#include <ModuleBase/Module.h>
#include <EventSystem/IEventListener.h>
#include <EngineCore/EngineExport.h>
#include <VideoSystem/VideoFrame.h>
#include <VideoSystem/VideoHandle.h>
#include <VideoSystem/iVideoBroker.h>
#include <cstdint>

class IVideoDecoderBackend;
class ResourceVideo;

class ModuleVideo : public Module, public IEventListener, public IVideoBroker
{
public:

    ModuleVideo(EventSystem* eventSystem, nous::engine::multithreading::NOUS_JobSystem* jobSystem);
    ~ModuleVideo() override;

    bool Awake() override;
    bool Start() override;
    UpdateStatus PreUpdate(float dt) override;
    UpdateStatus Update(float dt) override;
    UpdateStatus PostUpdate(float dt) override;
    bool CleanUp() override;

    void OnEvent(const Event& event) override;

    // Decoder lifecycle surface — what CVideoPlayer (Phase 3) and the F12 debug key drive.
    // Components hold the opaque handle and never include FFmpeg.
    NOUS_ENGINE_API VideoHandle CreateVideo(ResourceVideo* rVideo) const override;
    NOUS_ENGINE_API void        DestroyVideo(VideoHandle handle) const noexcept override;

    NOUS_ENGINE_API void        Start(VideoHandle handle) const override;
    NOUS_ENGINE_API void        Stop(VideoHandle handle) const;
    NOUS_ENGINE_API void        SetLooping(VideoHandle handle, bool looping) const override;

    NOUS_ENGINE_API bool        TryGetFrame(VideoHandle handle, double playheadSec, VideoFrame& out) const override;

    NOUS_ENGINE_API void        GetDimensions(VideoHandle handle, uint32_t& width, uint32_t& height) const;
    NOUS_ENGINE_API float       GetFrameRate(VideoHandle handle) const;
    NOUS_ENGINE_API float       GetDuration(VideoHandle handle) const override;
    NOUS_ENGINE_API bool        IsFinished(VideoHandle handle) const;

private:

    // Owned decode backend, created via CreateVideoDecoderBackend() in Awake — same shape as
    // RendererFrontend owning IRendererBackend* directly (the old VideoSystem passthrough facade
    // was deleted). Null if creation/init failed; every forwarder above no-ops in that case.
    IVideoDecoderBackend* m_backend;
};
