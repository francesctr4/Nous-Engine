#pragma once

#include "Engine/Systems/ECS/Component/Component.h"
#include "Engine/Systems/AudioSystem/SoundHandle.h"
#include "Engine/EngineExport.h"

class ResourceAudio;
class ModuleAudio;

/**
 * @brief 2D audio source component.
 *
 * Plays a single audio clip through the engine's audio backend. The component
 * owns exactly one backend voice (SoundHandle), created lazily the first time
 * the scene starts simulating. It never touches miniaudio directly — all calls
 * go through ModuleAudio (reached via the scene broker).
 *
 * Playback is driven from OnUpdate (NOT OnStart): in this engine OnStart fires
 * when the component is *added* in the editor, whereas "play on awake" means
 * "play when the scene starts playing". OnUpdate watches the simulation state
 * and starts/stops/syncs accordingly.
 */
class CAudioSource : public Component {
public:
    COMPONENT_TYPE(CAudioSource)

    // ---- Authoring fields (MVP — 2D only) ----
    ResourceAudio* clip        = nullptr;   // resolved from the asset on Deserialize / Inspector drop
    float          volume      = 1.0f;      // linear gain (1.0 = unchanged)
    float          pitch       = 1.0f;      // playback-rate multiplier (1.0 = unchanged)
    bool           loop        = false;
    bool           playOnAwake = true;

    // Lifecycle
    NOUS_ENGINE_API void OnUpdate(float deltaTime) override;
    NOUS_ENGINE_API void OnDestroy() override;

    // Serialization
    NOUS_ENGINE_API JsonObject Serialize() const override;
    NOUS_ENGINE_API void       Deserialize(const JsonObject& obj) override;

    // Editor auditioning — play the clip immediately (with the current volume/pitch)
    // without entering play mode. Uses a separate one-shot voice, independent of the
    // play-driven one. No-op without a clip / audio backend.
    NOUS_ENGINE_API void PreviewPlay();
    NOUS_ENGINE_API void PreviewStop();
    NOUS_ENGINE_API bool IsPreviewPlaying() const;

private:
    // Resolves the audio module through the scene broker (null in headless/test scenes).
    ModuleAudio* GetAudioModule() const;

    // Backend voice, created lazily on first play. Opaque token — no miniaudio here.
    SoundHandle m_sound        = nullptr;
    // Separate voice for the editor preview button (edit-mode auditioning).
    SoundHandle m_previewSound = nullptr;
    // True once playback has begun in the current play session; reset on STOP.
    bool        m_started      = false;
    // True while the voice is paused (cursor retained) so PLAYING can resume it.
    bool        m_paused       = false;
};
