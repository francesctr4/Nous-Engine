#include "Module.h"

#include "Logger.h"

Module::Module(Application* app, std::string name, bool start_enabled) : App(app), name(name), enabled(start_enabled)
{
	NOUS_TRACE("%s::%s()", name.c_str(), name.c_str());
}

Module::~Module()
{
	NOUS_TRACE("%s::~%s()", name.c_str(), name.c_str());
}

bool Module::Awake()
{
	NOUS_TRACE("%s::%s", name.c_str(), "Awake()");
	return true;
}

bool Module::Start()
{
	NOUS_TRACE("%s::%s", name.c_str(), "Start()");
	return true;
}

UpdateStatus Module::PreUpdate(float dt)
{
	NOUS_TRACE("%s::%s", name.c_str(), "PreUpdate()");
	return UPDATE_CONTINUE;
}

UpdateStatus Module::Update(float dt)
{
	NOUS_TRACE("%s::%s", name.c_str(), "Update()");
	return UPDATE_CONTINUE;
}

UpdateStatus Module::PostUpdate(float dt)
{
	NOUS_TRACE("%s::%s", name.c_str(), "PostUpdate()");
	return UPDATE_CONTINUE;
}

bool Module::CleanUp()
{
	NOUS_TRACE("%s::%s", name.c_str(), "CleanUp()");
	return true;
}