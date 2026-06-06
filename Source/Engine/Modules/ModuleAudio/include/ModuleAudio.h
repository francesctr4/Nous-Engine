#pragma once

#include "Engine/Modules/Module.h"
#include "Engine/Core/EventSystem/IEventListener.h"
#include "Engine/Core/Globals.h"
#include "Engine/EngineExport.h"
#include "Engine/Systems/AudioSystem/SoundHandle.h"

class IAudioEngineBackend;
class ResourceAudio;

class ModuleAudio : public Module, public IEventListener
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
    NOUS_ENGINE_API SoundHandle CreateSound(ResourceAudio* rAudio) const;
    NOUS_ENGINE_API void        DestroySound(SoundHandle sound) const noexcept;

    NOUS_ENGINE_API void        StartSound(SoundHandle sound) const;
    NOUS_ENGINE_API void        StopSound(SoundHandle sound) const;

    NOUS_ENGINE_API void        SetSoundVolume(SoundHandle sound, float volume) const;
    NOUS_ENGINE_API void        SetSoundPitch(SoundHandle sound, float pitch) const;
    NOUS_ENGINE_API void        SetSoundLooping(SoundHandle sound, bool looping) const;

    NOUS_ENGINE_API bool        IsSoundPlaying(SoundHandle sound) const;

private:

    // Owned audio backend, created via CreateAudioBackend() in Awake — same shape as
    // RendererFrontend owning IRendererBackend* / ModuleVideo owning IVideoDecoderBackend*
    // directly (the old AudioSystem passthrough facade was deleted). Null if creation/init
    // failed; every forwarder below no-ops (or returns null/false/0) in that case.
    IAudioEngineBackend* m_backend;

};
