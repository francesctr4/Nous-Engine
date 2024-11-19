#include "ModuleCamera3D.h"
#include "Logger.h"

ModuleCamera3D::ModuleCamera3D(Application* app, std::string name, bool start_enabled) : Module(app, name, start_enabled)
{
	NOUS_TRACE("%s()", __FUNCTION__);
}

ModuleCamera3D::~ModuleCamera3D()
{
	NOUS_TRACE("%s()", __FUNCTION__);
}

bool ModuleCamera3D::Awake()
{
	NOUS_TRACE("%s()", __FUNCTION__);
	return true;
}

bool ModuleCamera3D::Start()
{
	NOUS_TRACE("%s()", __FUNCTION__);
	return true;
}

UpdateStatus ModuleCamera3D::PreUpdate(float dt)
{
	NOUS_TRACE("%s()", __FUNCTION__);

	return UPDATE_CONTINUE;
}

UpdateStatus ModuleCamera3D::Update(float dt)
{
	NOUS_TRACE("%s()", __FUNCTION__);

	return UPDATE_CONTINUE;
}

UpdateStatus ModuleCamera3D::PostUpdate(float dt)
{
	NOUS_TRACE("%s()", __FUNCTION__);

	return UPDATE_CONTINUE;
}

bool ModuleCamera3D::CleanUp()
{
	NOUS_TRACE("%s()", __FUNCTION__);
	return true;
}