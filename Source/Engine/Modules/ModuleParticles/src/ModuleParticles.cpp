#include <ModuleParticles/ModuleParticles.h>

#include <EventSystem/Event.h>
#include <Logger/Logger.h>

constexpr LogChannel CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_MODULE_PARTICLES;

ModuleParticles::ModuleParticles(EventSystem* eventSystem, nous::engine::multithreading::NOUS_JobSystem* jobSystem)
    : Module(eventSystem, jobSystem)
{
}

ModuleParticles::~ModuleParticles() = default;

bool ModuleParticles::Awake()
{
    NOUS_INFO_C(CURRENT_CHANNEL, "Initializing Particle System ...");
    return true;
}

bool ModuleParticles::Start() { return true; }

UpdateStatus ModuleParticles::PreUpdate(float dt)  { return UpdateStatus::CONTINUE; }
UpdateStatus ModuleParticles::Update(float dt)     { return UpdateStatus::CONTINUE; }
UpdateStatus ModuleParticles::PostUpdate(float dt) { return UpdateStatus::CONTINUE; }

bool ModuleParticles::CleanUp()
{
    NOUS_INFO_C(CURRENT_CHANNEL, "Shutdown Particle System ...");
    return true;
}

void ModuleParticles::OnEvent(const Event& event)
{
}
