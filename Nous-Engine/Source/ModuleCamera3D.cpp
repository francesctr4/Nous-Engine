#include "ModuleCamera3D.h"
#include "ModuleInput.h"

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

	camera.SetPos(-100, 100, 300);

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

	float3 newPos(0, 0, 0);
	float speed = 20.0f * dt;
	float rotSensivity = 0.3f;
	float panSensivity = 3.6f;

	if (App->input->GetKey(SDL_SCANCODE_LSHIFT) == KeyState::REPEAT) speed *= 6;

	if (App->input->GetMouseButton(SDL_BUTTON_RIGHT) == KeyState::REPEAT && App->input->GetMouseButton(SDL_BUTTON_MIDDLE) == KeyState::IDLE) 
	{
		// WASD Camera Movement Handling
		HandleCameraMovement(newPos, speed);

		// Camera Rotation Handling
		HandleCameraRotation(rotSensivity, dt);

		if (App->input->GetKey(SDL_SCANCODE_LALT) == KeyState::REPEAT)
		{
			HandleCameraOrbit(newPos, rotSensivity, dt, float3::zero);
		}
	}

	if (App->input->GetMouseButton(SDL_BUTTON_MIDDLE) == KeyState::REPEAT) 
	{
		// Mouse wheel pressed while dragging movement handling
		HandleCameraPan(newPos, speed, panSensivity, dt);
	}

	HandleCameraZoom(newPos, speed);

	camera.UpdatePos(newPos);

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

Camera* ModuleCamera3D::GetCamera()
{
	return &camera;
}

void ModuleCamera3D::HandleCameraMovement(float3& newPos, const float& speed)
{
	if (App->input->GetKey(SDL_SCANCODE_W) == KeyState::REPEAT) newPos += camera.GetFront() * speed;
	if (App->input->GetKey(SDL_SCANCODE_S) == KeyState::REPEAT) newPos -= camera.GetFront() * speed;

	if (App->input->GetKey(SDL_SCANCODE_A) == KeyState::REPEAT) newPos -= camera.GetRight() * speed;
	if (App->input->GetKey(SDL_SCANCODE_D) == KeyState::REPEAT) newPos += camera.GetRight() * speed;

	if (App->input->GetKey(SDL_SCANCODE_E) == KeyState::REPEAT) newPos += camera.GetUp() * speed;
	if (App->input->GetKey(SDL_SCANCODE_Q) == KeyState::REPEAT) newPos -= camera.GetUp() * speed;
}

void ModuleCamera3D::HandleCameraRotation(const float& sensitivity, const float& dt)
{
	int dx = -App->input->GetMouseXMotion();
	int dy = -App->input->GetMouseYMotion();

	float s = sensitivity * dt;

	if (dx != 0)
	{
		float deltaX = (float)dx * s;

		float3 rotationAxis(0.0f, 1.0f, 0.0f);
		Quat rotationQuat = Quat::RotateAxisAngle(rotationAxis, deltaX);

		camera.SetUp((rotationQuat * camera.GetUp()).Normalized());
		camera.SetFront((rotationQuat * camera.GetFront()).Normalized());
	}
	if (dy != 0)
	{
		float deltaY = (float)dy * s;

		Quat rotationQuat = Quat::RotateAxisAngle(camera.GetRight(), deltaY);

		camera.SetUp((rotationQuat * camera.GetUp()).Normalized());
		camera.SetFront((rotationQuat * camera.GetFront()).Normalized());

		if (camera.GetUp().y < 0.0f) 
		{
			float3 front = camera.GetFront();
			front.y = std::clamp(front.y, -0.95f, 0.95f); // Prevent extreme angles.

			front.Normalize();
			camera.SetFront(front);

			// Recalculate 'Up' to maintain orthogonality.
			camera.SetUp(front.Cross(camera.GetRight()).Normalized());
		}
	}
}

void ModuleCamera3D::HandleCameraZoom(float3& newPos, const float& speed)
{
	if (App->input->GetMouseZ() > 0) newPos += camera.GetFront() * speed;
	if (App->input->GetMouseZ() < 0) newPos -= camera.GetFront() * speed;
}

void ModuleCamera3D::HandleCameraPan(float3& newPos, const float& speed, const float& sensitivity, const float& dt)
{
	int dx = -App->input->GetMouseXMotion();
	int dy = -App->input->GetMouseYMotion();

	float s = sensitivity * dt;

	float deltaX = (float)dx * s;
	float deltaY = (float)dy * s;

	newPos -= camera.GetUp() * speed * deltaY;
	newPos += camera.GetRight() * speed * deltaX;
}

void ModuleCamera3D::HandleCameraOrbit(float3& newPos, const float& sensitivity, const float& dt, const float3& lookAt)
{
	float distToRef = float3(lookAt - camera.GetPos()).Length();

	HandleCameraRotation(sensitivity, dt);

	camera.SetPos(lookAt + (camera.GetFront() * -distToRef));
}
