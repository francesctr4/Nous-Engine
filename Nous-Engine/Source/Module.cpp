#include "Module.h"

Module::Module(Application* app) : App(app)
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