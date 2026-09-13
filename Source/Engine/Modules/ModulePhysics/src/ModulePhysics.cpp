#include <ModulePhysics/ModulePhysics.h>

#include <EventSystem/Event.h>
#include <Logger/Logger.h>

constexpr LogChannel CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_MODULE_PHYSICS;

ModulePhysics::ModulePhysics(EventSystem* eventSystem, nous::engine::multithreading::NOUS_JobSystem* jobSystem)
    : Module(eventSystem, jobSystem)
{
}

ModulePhysics::~ModulePhysics() = default;

bool ModulePhysics::Awake()
{
    NOUS_INFO_C(CURRENT_CHANNEL, "Initializing Physics System ...");
    return true;
}

bool ModulePhysics::Start() { return true; }

UpdateStatus ModulePhysics::PreUpdate(float dt)  { return UpdateStatus::CONTINUE; }
UpdateStatus ModulePhysics::Update(float dt)     { return UpdateStatus::CONTINUE; }
UpdateStatus ModulePhysics::PostUpdate(float dt) { return UpdateStatus::CONTINUE; }

bool ModulePhysics::CleanUp()
{
    NOUS_INFO_C(CURRENT_CHANNEL, "Shutdown Physics System ...");
    return true;
}

void ModulePhysics::OnEvent(const Event& event)
{
}
