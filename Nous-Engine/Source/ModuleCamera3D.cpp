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

	static float z = 0.0f; // Static Z position of the camera
	// Move the camera backwards (z+) slightly each frame, scaled by dt for frame rate independence
	float movementSpeed = 0.01f; // Movement speed in units per second
	z += movementSpeed * dt; // Update the z position based on delta time

	camera.UpdatePos(float3(0, 0, z));

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

void ModuleCamera3D::ReceiveEvent(const Event& event)
{
	switch (event.type)
	{
		case EventType::WINDOW_RESIZED:
		{
			NOUS_DEBUG("%s() --> WINDOW RESIZED EVENT", __FUNCTION__);
			NOUS_DEBUG("Received context: %d, %d", event.context.int64[0], event.context.int64[1]);

			camera.SetAspectRatio((float)event.context.int64[0] / (float)event.context.int64[1]);

			break;
		}
	}
}
