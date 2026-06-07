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
    // Listener uniqueness: components pushed the listener during this frame's
    // Update phase (ModuleScene runs before ModuleAudio). >1 push means multiple
    // CAudioListeners have isMainListener set — last-writer-wins, but warn once.
    if (m_mainListenerPushes > 1)
    {
        if (!m_warnedMultipleListeners)
        {
            NOUS_WARN_C(CURRENT_CHANNEL,
                "%d active main AudioListeners this frame — only one should have "
                "isMainListener set. Last writer wins.", m_mainListenerPushes);
            m_warnedMultipleListeners = true;
        }
    }
    else
    {
        m_warnedMultipleListeners = false;  // config fixed (or went 0/1) — re-arm
    }

    m_mainListenerPushes = 0;
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

SoundHandle ModuleAudio::CreateSound(ResourceAudio* rAudio, AudioBus bus) const
{
    return m_backend ? m_backend->CreateSound(rAudio, bus) : nullptr;
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

// ---------------------------------------------------------------------------
// 3D spatialization (forwarded to the active backend)
// ---------------------------------------------------------------------------

void ModuleAudio::SetListenerPosition(float x, float y, float z)
{
    ++m_mainListenerPushes;  // counted per frame; evaluated in PostUpdate
    if (m_backend)
        m_backend->SetListenerPosition(x, y, z);
}

void ModuleAudio::SetListenerDirection(float x, float y, float z) const
{
    if (m_backend)
        m_backend->SetListenerDirection(x, y, z);
}

void ModuleAudio::SetListenerWorldUp(float x, float y, float z) const
{
    if (m_backend)
        m_backend->SetListenerWorldUp(x, y, z);
}

void ModuleAudio::SetSoundSpatializationEnabled(SoundHandle sound, bool enabled) const
{
    if (m_backend)
        m_backend->SetSoundSpatializationEnabled(sound, enabled);
}

void ModuleAudio::SetSoundPosition(SoundHandle sound, float x, float y, float z) const
{
    if (m_backend)
        m_backend->SetSoundPosition(sound, x, y, z);
}

void ModuleAudio::SetSoundMinDistance(SoundHandle sound, float distance) const
{
    if (m_backend)
        m_backend->SetSoundMinDistance(sound, distance);
}

void ModuleAudio::SetSoundMaxDistance(SoundHandle sound, float distance) const
{
    if (m_backend)
        m_backend->SetSoundMaxDistance(sound, distance);
}

void ModuleAudio::SetSoundAttenuationModel(SoundHandle sound, AttenuationModel model) const
{
    if (m_backend)
        m_backend->SetSoundAttenuationModel(sound, model);
}

// ---------------------------------------------------------------------------
// Bus mixer (forwarded to the active backend; guard-style no-ops / defaults)
// ---------------------------------------------------------------------------

void ModuleAudio::SetBusVolume(AudioBus bus, float volume) const
{
    if (m_backend) m_backend->SetBusVolume(bus, volume);
}

void ModuleAudio::SetBusMute(AudioBus bus, bool mute) const
{
    if (m_backend) m_backend->SetBusMute(bus, mute);
}

void ModuleAudio::SetBusSolo(AudioBus bus, bool solo) const
{
    if (m_backend) m_backend->SetBusSolo(bus, solo);
}

float ModuleAudio::GetBusVolume(AudioBus bus) const
{
    return m_backend ? m_backend->GetBusVolume(bus) : 1.0f;
}

bool ModuleAudio::GetBusMute(AudioBus bus) const
{
    return m_backend && m_backend->GetBusMute(bus);
}

bool ModuleAudio::GetBusSolo(AudioBus bus) const
{
    return m_backend && m_backend->GetBusSolo(bus);
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

