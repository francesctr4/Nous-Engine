#pragma once

#include "Engine/Systems/ECS/Component/Component.h"
#include "Engine/Systems/ECS/Component/Types/CAudioSource/include/AudioVoice.h"
#include "Engine/EngineExport.h"

class ResourceAudio;
class ModuleAudio;
class ModuleScene;

/**
 * @brief 2D audio source component.
 *
 * Plays a single audio clip through the engine's audio backend. The component
 * owns exactly one backend voice (AudioVoice, a move-only RAII handle), created
 * lazily the first time the scene starts simulating. It never touches miniaudio
 * directly — all calls go through ModuleAudio (reached via the scene broker).
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
    // Scene/broker resolution (both null in headless/test scenes). GetAudioModule
    // is GetModuleScene().GetAudio(); kept as a helper for the edit-mode preview path.
    ModuleScene* GetModuleScene() const;
    ModuleAudio* GetAudioModule() const;

    // OnUpdate split: edit-mode preview-voice maintenance vs the play-driven voice
    // state machine. Both run every frame once a scene + audio broker are resolved.
    void UpdatePreviewVoice(ModuleScene& moduleScene, ModuleAudio& audio);
    void UpdatePlaybackState(ModuleScene& moduleScene, ModuleAudio& audio);

    // Backend voice, created lazily on first play. RAII — releases itself on reset/destroy.
    AudioVoice m_sound;
    // Separate voice for the editor preview button (edit-mode auditioning).
    AudioVoice m_previewSound;
    // True once playback has begun in the current play session; reset on STOP.
    bool        m_started      = false;
    // True while the voice is paused (cursor retained) so PLAYING can resume it.
    bool        m_paused       = false;
};
