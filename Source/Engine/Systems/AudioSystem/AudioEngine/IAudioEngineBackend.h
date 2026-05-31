#pragma once

#include "Engine/Systems/AudioSystem/SoundHandle.h"

class ResourceAudio;

class IAudioEngineBackend
{
public:
    virtual ~IAudioEngineBackend() noexcept = default;

    virtual bool Initialize() = 0;
    virtual void PlayAudio(ResourceAudio* rAudio) = 0;
    virtual void Shutdown() noexcept = 0;

    // ----------------------------------------------------------------------
    // Per-voice sound lifecycle (used by CAudioSource).
    //
    // CreateSound builds a voice from the clip but does NOT start it.
    // The returned handle is owned by the caller and must be released with
    // DestroySound. All setters/queries no-op (or return false) on a null handle.
    // ----------------------------------------------------------------------
    virtual SoundHandle CreateSound(ResourceAudio* rAudio) = 0;
    virtual void        DestroySound(SoundHandle sound) noexcept = 0;

    virtual void        StartSound(SoundHandle sound) = 0;
    virtual void        StopSound(SoundHandle sound) = 0;

    virtual void        SetSoundVolume(SoundHandle sound, float volume) = 0;
    virtual void        SetSoundPitch(SoundHandle sound, float pitch) = 0;
    virtual void        SetSoundLooping(SoundHandle sound, bool looping) = 0;

    virtual bool        IsSoundPlaying(SoundHandle sound) const = 0;
};
