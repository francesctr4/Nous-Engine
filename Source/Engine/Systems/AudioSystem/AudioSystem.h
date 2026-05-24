#pragma once

#include "AudioSystemTypes.inl"

class IAudioEngineBackend;

class AudioSystem
{
public:

    AudioSystem();

    bool Initialize(AudioEngineBackend backend);
    void PlayAudio(ResourceAudio* rAudio) const;
    void Shutdown();

private:
    IAudioEngineBackend* m_audioEngine;
};
