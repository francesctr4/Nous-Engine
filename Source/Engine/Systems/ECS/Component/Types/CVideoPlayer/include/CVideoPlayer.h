#pragma once

#include "Engine/Systems/ECS/Component/Component.h"
#include "Engine/Systems/VideoSystem/VideoFrame.h"
#include "Engine/Systems/VideoSystem/VideoHandle.h"
#include "Engine/EngineExport.h"

#include <string>

class ResourceVideo;
class ModuleVideo;

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
    // Resolves the video module through the scene broker (null in headless/test scenes).
    ModuleVideo* GetVideoModule() const;

    bool m_started = false;   // play session has begun (gate one-time CreateVideo/Start)
};
