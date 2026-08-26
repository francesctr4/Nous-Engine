#pragma once

#include "Engine/Systems/ECS/Component/Component.h"
#include "Engine/Systems/ECS/Component/Types/CAudioSource/include/AudioVoice.h"
#include <AudioSystem/AudioTypes.h>
#include <AudioSystem/EffectChainHandle.h>
#include "Engine/EngineExport.h"

#include <cstdint>

class ResourceAudio;
class ResourceAudioGraph;
class ISceneHost;
class IAudioBroker;

/**
 * @brief 2D audio source component.
 *
 * Plays a single audio clip through the engine's audio backend. The component
 * owns exactly one backend voice (AudioVoice, a move-only RAII handle), created
 * lazily the first time the scene starts simulating. It never touches miniaudio
 * directly — all calls go through IAudioBroker, reached via Component::Services().
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

    // ---- 3D spatialization (Step 4) ----
    bool             spatialize  = false;   // 2D by default — preserves existing SFX + video-sync companion
    float            minDistance = 1.0f;
    float            maxDistance = 50.0f;
    AttenuationModel attenuation = AttenuationModel::Inverse;

    // ---- Bus routing (Step 6) ----
    // Mixer bus this source plays through. Read when the voice is (re)created, so
    // changing it mid-play applies on the next play session (see spec Limitations).
    AudioBus targetBus = AudioBus::SFX;

    // ---- Effect graph (Layer 2) ----
    // Optional DSP chain spliced between this source's voice and its bus. Resolved
    // from the .nafx asset on Deserialize / Inspector drop. null = dry (no effects).
    ResourceAudioGraph* effectGraph = nullptr;

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

    // Sibling-sync surface (read by a CVideoPlayer on the same GameObject):
    // IsVoicePlaying() is true only while the play-driven voice is actively
    // playing — it goes false once the clip finishes or is stopped, so a sibling
    // video stops slaving to a finished/idle voice and resumes its own loop clock.
    // GetPlaybackSeconds() returns that voice's cursor in seconds.
    [[nodiscard]] NOUS_ENGINE_API bool   IsVoicePlaying() const;
    [[nodiscard]] NOUS_ENGINE_API double GetPlaybackSeconds() const;

private:
    // OnUpdate split: edit-mode preview-voice maintenance vs the play-driven voice
    // state machine. Both run every frame once the host + audio broker are resolved.
    void UpdatePreviewVoice(const ISceneHost& host, const IAudioBroker& audio);
    void UpdatePlaybackState(const ISceneHost& host, const IAudioBroker& audio);

    // Builds/destroys the effect chain on the given voice. DestroyChain MUST run
    // before the matching voice is reset (the chain reattaches the voice to its bus
    // on teardown, so the voice must still be alive).
    void BuildChain(const IAudioBroker& audio, const AudioVoice& voice, EffectChainHandle& outChain);
    void DestroyChain(const IAudioBroker& audio, EffectChainHandle& chain);

    // Backend voice, created lazily on first play. RAII — releases itself on reset/destroy.
    AudioVoice m_sound;
    // Separate voice for the editor preview button (edit-mode auditioning).
    AudioVoice m_previewSound;

    // Effect chains spliced onto the play / preview voices (null = none).
    EffectChainHandle m_chain         = nullptr;
    EffectChainHandle m_previewChain  = nullptr;
    // ResourceAudioGraph::generation the play-voice chain was built from (live rebuild).
    uint32_t          m_chainGeneration = 0;
    // True once playback has begun in the current play session; reset on STOP.
    bool        m_started      = false;
    // True while the voice is paused (cursor retained) so PLAYING can resume it.
    bool        m_paused       = false;
};
