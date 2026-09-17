#include <ModuleAI/ModuleAI.h>

#include <EventSystem/Event.h>
#include <Logger/Logger.h>

constexpr LogChannel CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_MODULE_AI;

ModuleAI::ModuleAI(EventSystem* eventSystem, nous::engine::multithreading::NOUS_JobSystem* jobSystem)
    : Module(eventSystem, jobSystem)
{
}

ModuleAI::~ModuleAI() = default;

bool ModuleAI::Awake()
{
    NOUS_INFO_C(CURRENT_CHANNEL, "Initializing AI System ...");
    return true;
}

bool ModuleAI::Start() { return true; }

UpdateStatus ModuleAI::PreUpdate(float dt)  { return UpdateStatus::CONTINUE; }
UpdateStatus ModuleAI::Update(float dt)     { return UpdateStatus::CONTINUE; }
UpdateStatus ModuleAI::PostUpdate(float dt) { return UpdateStatus::CONTINUE; }

bool ModuleAI::CleanUp()
{
    NOUS_INFO_C(CURRENT_CHANNEL, "Shutdown AI System ...");
    return true;
}

void ModuleAI::OnEvent(const Event& event)
{
}
