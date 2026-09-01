#include <ModuleAnimation/ModuleAnimation.h>

#include <EventSystem/Event.h>
#include <Logger/Logger.h>

constexpr LogChannel CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_MODULE_ANIMATION;

ModuleAnimation::ModuleAnimation(EventSystem* eventSystem, nous::engine::multithreading::NOUS_JobSystem* jobSystem)
    : Module(eventSystem, jobSystem)
{
}

ModuleAnimation::~ModuleAnimation() = default;

bool ModuleAnimation::Awake()
{
    NOUS_INFO_C(CURRENT_CHANNEL, "Initializing Animation System ...");
    return true;
}

bool ModuleAnimation::Start() { return true; }

UpdateStatus ModuleAnimation::PreUpdate(float dt)  { return UpdateStatus::CONTINUE; }
UpdateStatus ModuleAnimation::Update(float dt)     { return UpdateStatus::CONTINUE; }
UpdateStatus ModuleAnimation::PostUpdate(float dt) { return UpdateStatus::CONTINUE; }

bool ModuleAnimation::CleanUp()
{
    NOUS_INFO_C(CURRENT_CHANNEL, "Shutdown Animation System ...");
    return true;
}

void ModuleAnimation::OnEvent(const Event& event)
{
}
