#pragma once

#include <ECS/Component/Component.h>
#include <VideoSystem/VideoFrame.h>
#include <VideoSystem/VideoHandle.h>
#include <EngineCore/EngineExport.h>

#include <string>

class ResourceVideo;

/**
 * @brief In-world video surface component.
 *
 * Decodes a ResourceVideo through IVideoBroker and exposes the latest RGBA frame.
 * It owns exactly one backend decoder handle (VideoHandle), created lazily when the
 * scene starts simulating, and never touches FFmpeg or the GPU directly. The renderer
 * (ModuleRenderer3D's drain → RendererFrontend's DynamicTextureCache) reads `latestFrame` each
 * frame, uploads it to a renderer-owned dynamic texture, and binds it into the sibling
 * CMaterial's `targetSlot`.
 *
 * Playback is driven from OnUpdate (NOT OnStart), watching the ISceneHost's simulation
 * state exactly like CAudioSource.
 */
class CVideoPlayer : public Component {
public:
    COMPONENT_TYPE(CVideoPlayer)

    // ---- Authoring fields ----
    ResourceVideo* clip        = nullptr;          // resolved on Deserialize / Inspector drop
    bool           loop        = false;
    bool           playOnAwake = true;
    bool           syncToAudio = true;             // slave the playhead to a sibling CAudioSource's
                                                   // clock for A/V sync — only when this clip has an
                                                   // audio track (never for GIFs). Uncheck to run the
                                                   // video on its own clock, independent of the audio.
    bool           predecode   = false;            // decode every frame into memory up front instead of
                                                   // streaming. Trades RAM (frames * w*h*4) for a SEAMLESS
                                                   // loop — the streamed path stutters at the loop seam
                                                   // because it must seek back to frame 0. Use only for
                                                   // short, low-res clips (a long/4K clip would OOM).
    std::string    targetSlot  = "diffuseSampler"; // material textureMaps key the video drives

    // ---- Runtime state (not serialized; read by ModuleRenderer3D) ----
    VideoHandle handle     = nullptr;   // owned; released via the IVideoBroker
    double      playhead   = 0.0;
    VideoFrame  latestFrame{};          // latched ptr from TryGetFrame (valid until next call)
    bool        frameDirty = false;     // a new frame is waiting to be uploaded

    // True while this player is expected to start on play but hasn't finished its one-time
    // load attempt yet (for PREDECODED clips that's the synchronous full-decode). A sibling
    // CAudioSource uses this to delay its own start so audio and video begin together.
    // Becomes false once the load attempt completes (whether it succeeded or failed), so a
    // failed load never blocks the audio forever.
    [[nodiscard]] bool IsLoadingForPlayback() const
    {
        return playOnAwake && clip != nullptr && !m_started;
    }

    // Lifecycle
    NOUS_ENGINE_API void OnUpdate(float deltaTime) override;
    NOUS_ENGINE_API void OnDestroy() override;

    // Serialization
    NOUS_ENGINE_API JsonObject Serialize() const override;
    NOUS_ENGINE_API void       Deserialize(const JsonObject& obj) override;

private:
    bool m_started = false;   // play session has begun (gate one-time CreateVideo/Start)
};
