#pragma once

#include "Engine/Systems/AudioSystem/AudioEngine/IAudioEngineBackend.h"

#include <miniaudio.h>

class ResourceAudio;

class MiniaudioBackend : public IAudioEngineBackend
{
public:

    MiniaudioBackend();
    ~MiniaudioBackend() override;

    bool Initialize() override;
    void PlayAudio(ResourceAudio* rAudio) override;
    void Shutdown() noexcept override;

    // Per-voice sound lifecycle. The handle is a heap-allocated ma_sound.
    SoundHandle CreateSound(ResourceAudio* rAudio) override;
    void        DestroySound(SoundHandle sound) noexcept override;

    void        StartSound(SoundHandle sound) override;
    void        StopSound(SoundHandle sound) override;

    void        SetSoundVolume(SoundHandle sound, float volume) override;
    void        SetSoundPitch(SoundHandle sound, float pitch) override;
    void        SetSoundLooping(SoundHandle sound, bool looping) override;

    bool        IsSoundPlaying(SoundHandle sound) const override;

    double      GetCursorSeconds(SoundHandle sound) const override;

private:

    ma_engine m_audioEngine;

};
