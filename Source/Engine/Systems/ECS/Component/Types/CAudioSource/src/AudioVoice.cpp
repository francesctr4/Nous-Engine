#include "Engine/Systems/ECS/Component/Types/CAudioSource/include/AudioVoice.h"

#include "Engine/Modules/ModuleAudio/include/ModuleAudio.h"

AudioVoice::AudioVoice(ModuleAudio* audio, SoundHandle handle)
    : m_audio(audio), m_handle(handle)
{
}

AudioVoice::~AudioVoice()
{
    Reset();
}

AudioVoice::AudioVoice(AudioVoice&& other) noexcept
    : m_audio(other.m_audio), m_handle(other.m_handle)
{
    other.m_handle = nullptr;
}

AudioVoice& AudioVoice::operator=(AudioVoice&& other) noexcept
{
    if (this != &other)
    {
        Reset();
        m_audio        = other.m_audio;
        m_handle       = other.m_handle;
        other.m_handle = nullptr;
    }
    return *this;
}

void AudioVoice::Reset() noexcept
{
    if (m_handle && m_audio)
        m_audio->DestroySound(m_handle);
    m_handle = nullptr;
}
