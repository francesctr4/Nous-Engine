#include "AudioSystem.h"

#include "AudioEngine/Backends/miniaudio/MiniaudioBackend.h"
#include "AudioEngine/IAudioEngineBackend.h"
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
            m_audioEngine = NOUS_NEW<MiniaudioBackend>(MemoryTag::AUDIO_SYSTEM);
            break;
        case AudioEngineBackend::UNKNOWN:
        default:
            NOUS_ERROR_C(CURRENT_CHANNEL, "Invalid audio backend requested.");
            return false;
    }

    if (!m_audioEngine->Initialize())
    {
        NOUS_DELETE(m_audioEngine, MemoryTag::AUDIO_SYSTEM);
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

    NOUS_DELETE(m_audioEngine, MemoryTag::AUDIO_SYSTEM);
}

// ---------------------------------------------------------------------------
// Per-voice sound lifecycle (thin forwarding to the active backend)
// ---------------------------------------------------------------------------

SoundHandle AudioSystem::CreateSound(ResourceAudio* rAudio) const
{
    return m_audioEngine ? m_audioEngine->CreateSound(rAudio) : nullptr;
}

void AudioSystem::DestroySound(SoundHandle sound) const noexcept
{
    if (m_audioEngine)
        m_audioEngine->DestroySound(sound);
}

void AudioSystem::StartSound(SoundHandle sound) const
{
    if (m_audioEngine)
        m_audioEngine->StartSound(sound);
}

void AudioSystem::StopSound(SoundHandle sound) const
{
    if (m_audioEngine)
        m_audioEngine->StopSound(sound);
}

void AudioSystem::SetSoundVolume(SoundHandle sound, float volume) const
{
    if (m_audioEngine)
        m_audioEngine->SetSoundVolume(sound, volume);
}

void AudioSystem::SetSoundPitch(SoundHandle sound, float pitch) const
{
    if (m_audioEngine)
        m_audioEngine->SetSoundPitch(sound, pitch);
}

void AudioSystem::SetSoundLooping(SoundHandle sound, bool looping) const
{
    if (m_audioEngine)
        m_audioEngine->SetSoundLooping(sound, looping);
}

bool AudioSystem::IsSoundPlaying(SoundHandle sound) const
{
    return m_audioEngine && m_audioEngine->IsSoundPlaying(sound);
}
