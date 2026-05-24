#include "AudioSystem.h"

#include "AudioEngine/Backends/miniaudio/MiniaudioBackend.h"
#include "AudioEngine/IAudioEngineBackend.h"
#include "AudioSystemTypes.inl"
#include "Engine/Core/Logger/Asserts.h"
#include "Engine/Core/Logger/Logger.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"

constexpr LogChannel CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_SYSTEM_AUDIOSYSTEM;

AudioSystem::AudioSystem(): m_audioEngine(nullptr)
{

}

bool AudioSystem::Initialize(AudioEngineBackend backend)
{
    NOUS_ASSERT(m_audioEngine == nullptr);  // no double-init

    switch (backend)
    {
        case AudioEngineBackend::MINIAUDIO:
            NOUS_INFO_C(CURRENT_CHANNEL, "Using audio backend: MINIAUDIO");
            m_audioEngine = NOUS_NEW<MiniaudioBackend>(MemoryTag::AUDIO);
            break;
        case AudioEngineBackend::UNKNOWN:
        default:
            NOUS_ERROR_C(CURRENT_CHANNEL, "Invalid audio backend requested.");
            return false;
    }

    if (!m_audioEngine->Initialize())
    {
        NOUS_DELETE(m_audioEngine, MemoryTag::AUDIO);
        return false;
    }

    return true;
}

void AudioSystem::PlayAudio(ResourceAudio* rAudio) const
{
    if (m_audioEngine)
        m_audioEngine->PlayAudio(rAudio);
}

void AudioSystem::Shutdown()
{
    if (m_audioEngine)
        m_audioEngine->Shutdown();

    NOUS_DELETE(m_audioEngine, MemoryTag::AUDIO);
}
