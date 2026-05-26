#include "Engine/Modules/ModuleAudio/include/ModuleAudio.h"

#include "Engine/Core/Logger/Logger.h"
#include "Engine/Core/EventSystem/EventSystem.h"
#include "Engine/Core/EventSystem/Event/include/Event.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"

#include "Engine/Systems/AudioSystem/AudioSystem.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceAudio/include/ResourceAudio.h"

constexpr LogChannel CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_MODULE_AUDIO;

ModuleAudio::ModuleAudio(EventSystem* eventSystem, nous::engine::multithreading::NOUS_JobSystem* jobSystem)
    : Module(eventSystem, jobSystem), m_audioSystem(nullptr)
{

}

ModuleAudio::~ModuleAudio() = default;

bool ModuleAudio::Awake()
{
    NOUS_INFO_C(CURRENT_CHANNEL, "Initializing Audio System ...");

    m_audioSystem = NOUS_NEW<AudioSystem>(MemoryTag::AUDIO_SYSTEM);

    if (!m_audioSystem->Initialize(AudioEngineBackend::MINIAUDIO))
        NOUS_WARN_C(CURRENT_CHANNEL, "Audio system initialization failed — running without audio.");

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

    m_audioSystem->Shutdown();
    NOUS_DELETE<AudioSystem>(m_audioSystem, MemoryTag::AUDIO_SYSTEM);

	return true;
}

void ModuleAudio::PlayAudio(ResourceAudio* rAudio) const
{
    if (m_audioSystem && rAudio)
        m_audioSystem->PlayAudio(rAudio);
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

