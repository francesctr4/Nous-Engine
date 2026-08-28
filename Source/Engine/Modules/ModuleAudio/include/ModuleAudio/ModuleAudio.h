#pragma once

#include <ModuleBase/Module.h>
#include <EventSystem/IEventListener.h>
#include <EngineCore/EngineExport.h>
#include <AudioSystem/SoundHandle.h>
#include <AudioSystem/EffectChainHandle.h>
#include <AudioSystem/AudioTypes.h>
#include <AudioSystem/AudioGraph/AudioEffectTypes.h>
#include <AudioSystem/iAudioBroker.h>

class IAudioEngineBackend;
class ResourceAudio;

class ModuleAudio : public Module, public IEventListener, public IAudioBroker
{
public:

    // Constructor
    ModuleAudio(EventSystem* eventSystem, nous::engine::multithreading::NOUS_JobSystem* jobSystem);

    // Destructor
    ~ModuleAudio() override;

    bool Awake() override;
    bool Start() override;
    UpdateStatus PreUpdate(float dt) override;
    UpdateStatus Update(float dt) override;
    UpdateStatus PostUpdate(float dt) override;
    bool CleanUp() override;

    void OnEvent(const Event& event) override;

    // Fire-and-forget one-shot: the engine owns and self-cleans the voice, so the
    // caller gets NO handle and NO control (stop/volume/pitch/loop). For anything
    // beyond a debug ping (gameplay, music) use the CreateSound voice API below.
    NOUS_ENGINE_API void PlayAudio(ResourceAudio* rAudio) const;

    // ----------------------------------------
    // Per-voice sound lifecycle — the surface CAudioSource drives. Components
    // hold the returned opaque handle and never touch miniaudio directly.
    // ----------------------------------------
    NOUS_ENGINE_API SoundHandle CreateSound(ResourceAudio* rAudio, AudioBus bus) const override;
    NOUS_ENGINE_API void        DestroySound(SoundHandle sound) const noexcept override;

    NOUS_ENGINE_API void        StartSound(SoundHandle sound) const override;
    NOUS_ENGINE_API void        StopSound(SoundHandle sound) const override;

    NOUS_ENGINE_API void        SetSoundVolume(SoundHandle sound, float volume) const override;
    NOUS_ENGINE_API void        SetSoundPitch(SoundHandle sound, float pitch) const override;
    NOUS_ENGINE_API void        SetSoundLooping(SoundHandle sound, bool looping) const override;

    NOUS_ENGINE_API bool        IsSoundPlaying(SoundHandle sound) const override;

    // Playback position of the voice in seconds (0 when no backend / null handle).
    NOUS_ENGINE_API double      GetCursorSeconds(SoundHandle sound) const override;

    // ----------------------------------------
    // 3D spatialization (forwarded to the active backend). The main CAudioListener
    // writes the listener each frame; CAudioSource writes its voice's position +
    // attenuation. All no-op when there is no backend.
    // ----------------------------------------
    NOUS_ENGINE_API void SetListenerPosition (float x, float y, float z) override;
    NOUS_ENGINE_API void SetListenerDirection(float x, float y, float z) const override;
    NOUS_ENGINE_API void SetListenerWorldUp  (float x, float y, float z) const override;

    NOUS_ENGINE_API void SetSoundSpatializationEnabled(SoundHandle sound, bool enabled) const override;
    NOUS_ENGINE_API void SetSoundPosition        (SoundHandle sound, float x, float y, float z) const override;
    NOUS_ENGINE_API void SetSoundMinDistance     (SoundHandle sound, float distance) const override;
    NOUS_ENGINE_API void SetSoundMaxDistance     (SoundHandle sound, float distance) const override;
    NOUS_ENGINE_API void SetSoundAttenuationModel(SoundHandle sound, AttenuationModel model) const override;

    // ----------------------------------------
    // Bus mixer (Step 6) — driven by the editor AudioMixerWindow. Getters return
    // safe defaults (1.0 / false) when there is no backend.
    // ----------------------------------------
    NOUS_ENGINE_API void  SetBusVolume(AudioBus bus, float volume) const;
    NOUS_ENGINE_API void  SetBusMute  (AudioBus bus, bool mute) const;
    NOUS_ENGINE_API void  SetBusSolo  (AudioBus bus, bool solo) const;
    NOUS_ENGINE_API float GetBusVolume(AudioBus bus) const;
    NOUS_ENGINE_API bool  GetBusMute  (AudioBus bus) const;
    NOUS_ENGINE_API bool  GetBusSolo  (AudioBus bus) const;

    // ----------------------------------------
    // Effect chain (forwarded to the active backend; guard-style no-ops).
    // ----------------------------------------
    NOUS_ENGINE_API EffectChainHandle CreateEffectChain(SoundHandle sound, const AudioGraphDesc& desc, AudioBus outputBus) const override;
    NOUS_ENGINE_API void              SetEffectParam(EffectChainHandle chain, int effectIndex, int paramIndex, float value) const override;
    NOUS_ENGINE_API void              DestroyEffectChain(EffectChainHandle chain) const noexcept override;

private:

    // Owned audio backend, created via CreateAudioBackend() in Awake — same shape as
    // RendererFrontend owning IRendererBackend* / ModuleVideo owning IVideoDecoderBackend*
    // directly (the old AudioSystem passthrough facade was deleted). Null if creation/init
    // failed; every forwarder below no-ops (or returns null/false/0) in that case.
    IAudioEngineBackend* m_backend;

    // Per-frame count of main-listener pushes (incremented by SetListenerPosition,
    // evaluated + reset in PostUpdate). Warns once if >1 listener is active.
    int  m_mainListenerPushes      = 0;
    bool m_warnedMultipleListeners = false;

};
