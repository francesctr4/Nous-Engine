#include "Module.h"

Module::Module(Application* app, bool start_enabled) : App(app), enabled(start_enabled)
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