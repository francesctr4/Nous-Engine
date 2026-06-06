#include "Engine/Modules/ModuleAudio/include/ModuleAudio.h"

#include "Engine/Core/Logger/Logger.h"
#include "Engine/Core/EventSystem/EventSystem.h"
#include "Engine/Core/EventSystem/Event/include/Event.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"

#include "Engine/Systems/AudioSystem/AudioEngine/AudioBackendFactory.h"
#include "Engine/Systems/AudioSystem/AudioEngine/IAudioEngineBackend.h"
#include "Engine/Systems/ResourceManager/Types/ResourceAudio/include/ResourceAudio.h"

constexpr LogChannel CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_MODULE_AUDIO;

ModuleAudio::ModuleAudio(EventSystem* eventSystem, nous::engine::multithreading::NOUS_JobSystem* jobSystem)
    : Module(eventSystem, jobSystem), m_backend(nullptr)
{

}

ModuleAudio::~ModuleAudio() = default;

bool ModuleAudio::Awake()
{
    NOUS_INFO_C(CURRENT_CHANNEL, "Initializing Audio System ...");

    m_backend = CreateAudioBackend(AudioEngineBackend::MINIAUDIO);

    if (!m_backend || !m_backend->Initialize())
    {
        NOUS_WARN_C(CURRENT_CHANNEL, "Audio system initialization failed — running without audio.");
        if (m_backend)
        {
            NOUS_DELETE(m_backend, MemoryTag::AUDIO_SYSTEM);
            m_backend = nullptr;
        }
    }

    return true;
}

bool ModuleAudio::Start()
{
    return true;
}

UpdateStatus ModuleAudio::PreUpdate(float dt)
{
    return UpdateStatus::CONTINUE;
}

UpdateStatus ModuleAudio::Update(float dt)
{
    return UpdateStatus::CONTINUE;
}

UpdateStatus ModuleAudio::PostUpdate(float dt)
{
    return UpdateStatus::CONTINUE;
}

bool ModuleAudio::CleanUp()
{
    NOUS_INFO_C(CURRENT_CHANNEL, "Shutdown Audio System ...");

    if (m_backend)
    {
        m_backend->Shutdown();
        NOUS_DELETE(m_backend, MemoryTag::AUDIO_SYSTEM);
        m_backend = nullptr;
    }

	return true;
}

void ModuleAudio::PlayAudio(ResourceAudio* rAudio) const
{
    if (m_backend && rAudio)
        m_backend->PlayAudio(rAudio);
}

// ---------------------------------------------------------------------------
// Per-voice sound lifecycle (forwarded to the active backend)
// ---------------------------------------------------------------------------

SoundHandle ModuleAudio::CreateSound(ResourceAudio* rAudio) const
{
    return m_backend ? m_backend->CreateSound(rAudio) : nullptr;
}

void ModuleAudio::DestroySound(SoundHandle sound) const noexcept
{
    if (m_backend)
        m_backend->DestroySound(sound);
}

void ModuleAudio::StartSound(SoundHandle sound) const
{
    if (m_backend)
        m_backend->StartSound(sound);
}

void ModuleAudio::StopSound(SoundHandle sound) const
{
    if (m_backend)
        m_backend->StopSound(sound);
}

void ModuleAudio::SetSoundVolume(SoundHandle sound, float volume) const
{
    if (m_backend)
        m_backend->SetSoundVolume(sound, volume);
}

void ModuleAudio::SetSoundPitch(SoundHandle sound, float pitch) const
{
    if (m_backend)
        m_backend->SetSoundPitch(sound, pitch);
}

void ModuleAudio::SetSoundLooping(SoundHandle sound, bool looping) const
{
    if (m_backend)
        m_backend->SetSoundLooping(sound, looping);
}

bool ModuleAudio::IsSoundPlaying(SoundHandle sound) const
{
    return m_backend && m_backend->IsSoundPlaying(sound);
}

double ModuleAudio::GetCursorSeconds(SoundHandle sound) const
{
    return m_backend ? m_backend->GetCursorSeconds(sound) : 0.0;
}

void ModuleAudio::OnEvent(const Event &event)
{
    switch (event.type)
    {
        default:
            {
                break;
            }
    }
}

