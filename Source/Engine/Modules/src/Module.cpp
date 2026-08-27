#include <ModuleBase/Module.h>

Module::Module(EventSystem* eventSystem, nous::engine::multithreading::NOUS_JobSystem* jobSystem) :
    eventSystem(eventSystem),
    JobSystem(jobSystem)
{}

Module::~Module() = default;

bool Module::Awake()
{
	return true;
}

bool Module::Start()
{
	return true;
}

UpdateStatus Module::PreUpdate(float dt)
{
	return UpdateStatus::CONTINUE;
}

UpdateStatus Module::Update(float dt)
{
	return UpdateStatus::CONTINUE;
}

UpdateStatus Module::PostUpdate(float dt)
{
	return UpdateStatus::CONTINUE;
}

bool Module::CleanUp()
{
	return true;
}