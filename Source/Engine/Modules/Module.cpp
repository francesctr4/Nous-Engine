#include "Module.h"

Module::Module(EventSystem* eventSystem, NOUS_Multithreading::NOUS_JobSystem* jobSystem, bool isGameMode) :
    eventSystem(eventSystem),
    JobSystem(jobSystem),
    IsGameMode(isGameMode)
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