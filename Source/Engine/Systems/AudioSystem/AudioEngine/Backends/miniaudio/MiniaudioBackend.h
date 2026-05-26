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

private:

    ma_engine m_audioEngine;

};
