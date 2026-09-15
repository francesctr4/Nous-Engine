#include <ModuleUI/ModuleUI.h>

#include <EventSystem/Event.h>
#include <Logger/Logger.h>

constexpr LogChannel CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_MODULE_UI;

ModuleUI::ModuleUI(EventSystem* eventSystem, nous::engine::multithreading::NOUS_JobSystem* jobSystem)
    : Module(eventSystem, jobSystem)
{
}

ModuleUI::~ModuleUI() = default;

bool ModuleUI::Awake()
{
    NOUS_INFO_C(CURRENT_CHANNEL, "Initializing UI System ...");
    return true;
}

bool ModuleUI::Start() { return true; }

UpdateStatus ModuleUI::PreUpdate(float dt)  { return UpdateStatus::CONTINUE; }
UpdateStatus ModuleUI::Update(float dt)     { return UpdateStatus::CONTINUE; }
UpdateStatus ModuleUI::PostUpdate(float dt) { return UpdateStatus::CONTINUE; }

bool ModuleUI::CleanUp()
{
    NOUS_INFO_C(CURRENT_CHANNEL, "Shutdown UI System ...");
    return true;
}

void ModuleUI::OnEvent(const Event& event)
{
}
