#pragma once

#include "Engine/Systems/ECS/Component/Component.h"
#include "Engine/Systems/VideoSystem/VideoFrame.h"
#include "Engine/Systems/VideoSystem/VideoHandle.h"
#include "Engine/EngineExport.h"

#include <string>

class ResourceVideo;
class ModuleVideo;
class ModuleScene;
class Scene;

/**
 * @brief In-world video surface component.
 *
 * Decodes a ResourceVideo through ModuleVideo and exposes the latest RGBA frame.
 * It owns exactly one backend decoder handle (VideoHandle), created lazily when the
 * scene starts simulating, and never touches FFmpeg or the GPU directly. The renderer
 * (ModuleRenderer3D's drain → RendererFrontend's DynamicTextureCache) reads `latestFrame` each
 * frame, uploads it to a renderer-owned dynamic texture, and binds it into the sibling
 * CMaterial's `targetSlot`.
 *
 * Playback is driven from OnUpdate (NOT OnStart), watching ModuleScene's simulation state
 * exactly like CAudioSource.
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
    std::string    targetSlot  = "diffuseSampler"; // material textureMaps key the video drives

    // ---- Runtime state (not serialized; read by ModuleRenderer3D) ----
    VideoHandle handle     = nullptr;   // owned; released via the ModuleVideo broker
    double      playhead   = 0.0;
    VideoFrame  latestFrame{};          // latched ptr from TryGetFrame (valid until next call)
    bool        frameDirty = false;     // a new frame is waiting to be uploaded

    // Lifecycle
    NOUS_ENGINE_API void OnUpdate(float deltaTime) override;
    NOUS_ENGINE_API void OnDestroy() override;

    // Serialization
    NOUS_ENGINE_API JsonObject Serialize() const override;
    NOUS_ENGINE_API void       Deserialize(const JsonObject& obj) override;

private:
    // The scene-broker chain this component talks to, resolved in one walk. Any field may be
    // null in a headless / test scene (no scene, or no ModuleVideo wired). `video` non-null
    // implies `scene` and `moduleScene` are non-null too (each is derived from the previous).
    struct SceneBroker
    {
        Scene*       scene       = nullptr;
        ModuleScene* moduleScene = nullptr;
        ModuleVideo* video       = nullptr;
    };
    // Walks GameObject -> Scene -> ModuleScene -> ModuleVideo once. Used by OnUpdate/OnDestroy
    // instead of re-deriving the chain inline (single source of truth for the broker path).
    [[nodiscard]] SceneBroker ResolveBroker() const;

    bool m_started = false;   // play session has begun (gate one-time CreateVideo/Start)
};
