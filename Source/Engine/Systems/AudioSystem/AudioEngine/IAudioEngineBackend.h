#pragma once

#include "Engine/Systems/AudioSystem/AudioSystemTypes.inl"

class IAudioEngineBackend
{
public:
    virtual ~IAudioEngineBackend() noexcept = default;

    virtual bool Initialize() = 0;
    virtual void PlayAudio(ResourceAudio* rAudio) = 0;
    virtual void Shutdown() noexcept = 0;
};
