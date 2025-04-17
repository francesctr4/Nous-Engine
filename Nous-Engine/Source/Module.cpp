#include "Module.h"

#include "Logger.h"

Module::Module(Application* app, std::string name, bool start_enabled) : App(app), name(name), enabled(start_enabled)
{
}

Module::~Module()
{
}

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
	return UPDATE_CONTINUE;
}

UpdateStatus Module::Update(float dt)
{
	return UPDATE_CONTINUE;
}

UpdateStatus Module::PostUpdate(float dt)
{
	return UPDATE_CONTINUE;
}

bool Module::CleanUp()
{
	return true;
}

void Module::ReceiveEvent(const Event& event)
{

}

std::string Module::GetName()
{
	return name;
}