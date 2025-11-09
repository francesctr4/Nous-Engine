#include "Engine/Modules/ModuleCamera3D/include/ModuleCamera3D.h"
#include "Engine/Modules/ModuleInput/include/ModuleInput.h"
#include "Engine/Core/LoggingSystem/Logger.h"
#include "Engine/Core/Application.h"
#include "Engine/Systems/CameraSystem/Camera/include/Camera.h"
#include <algorithm>

// SDL3
#include "SDL3/SDL.h"

#include "Engine/Core/EventSystem/EventSystem.h"

#include "glm/gtc/quaternion.hpp"
#include "Engine/Core/MemoryManager/MemoryManager.h"

ModuleCamera3D::ModuleCamera3D(Application* app) : Module(app)
{
	NOUS_TRACE("%s()", __FUNCTION__);
	sceneViewportHovered = false;

	camera = NOUS_NEW<Camera>(MemoryManager::MemoryTag::ENTITY);

    App->eventSystem->Subscribe(EventType::WINDOW_RESIZED, this);
}

ModuleCamera3D::~ModuleCamera3D()
{
	NOUS_TRACE("%s()", __FUNCTION__);
	NOUS_DELETE(camera, MemoryManager::MemoryTag::ENTITY);
}

bool ModuleCamera3D::Awake()
{
	NOUS_TRACE("%s()", __FUNCTION__);

	camera->SetPos(-100, 100, 300);

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

	return UpdateStatus::CONTINUE;
}

UpdateStatus ModuleCamera3D::Update(float dt)
{
	NOUS_TRACE("%s()", __FUNCTION__);

	if (sceneViewportHovered) 
	{
		glm::vec3 newPos(0, 0, 0);
		float speed = 20.0f * dt;
		float rotSensitivity = 0.3f;
		float panSensitivity = 3.6f;

		if (App->input->GetKey(SDL_SCANCODE_LSHIFT) == KeyState::REPEAT) speed *= 6;

		if (App->input->GetMouseButton(SDL_BUTTON_RIGHT) == KeyState::REPEAT && App->input->GetMouseButton(SDL_BUTTON_MIDDLE) == KeyState::IDLE)
		{
			// WASD Camera Movement Handling
			HandleCameraMovement(newPos, speed);

			// Camera Rotation Handling
			HandleCameraRotation(rotSensitivity, dt);

			if (App->input->GetKey(SDL_SCANCODE_LALT) == KeyState::REPEAT)
			{
				HandleCameraOrbit(rotSensitivity, dt, glm::vec3(0.0f));
			}
		}

		if (App->input->GetMouseButton(SDL_BUTTON_MIDDLE) == KeyState::REPEAT)
		{
			// Mouse wheel pressed while dragging movement handling
			HandleCameraPan(newPos, speed, panSensitivity, dt);
		}

		HandleCameraZoom(newPos, speed);

		camera->UpdatePos(newPos);
	}

	return UpdateStatus::CONTINUE;
}

UpdateStatus ModuleCamera3D::PostUpdate(float dt)
{
	NOUS_TRACE("%s()", __FUNCTION__);

	return UpdateStatus::CONTINUE;
}

bool ModuleCamera3D::CleanUp()
{
	NOUS_TRACE("%s()", __FUNCTION__);
	return true;
}

void ModuleCamera3D::OnEvent(const Event& event)
{
	switch (event.type)
	{
		case EventType::WINDOW_RESIZED:
		{
			NOUS_DEBUG("%s() --> WINDOW RESIZED EVENT", __FUNCTION__);
			NOUS_DEBUG("Received context: %d, %d", event.ctx.i32[0], event.ctx.i32[1]);

			camera->SetAspectRatio((float)event.ctx.i32[0] / (float)event.ctx.i32[1]);

			break;
		}
    }
}

Camera* ModuleCamera3D::GetCamera()
{
	return camera;
}

void ModuleCamera3D::HandleCameraMovement(glm::vec3& newPos, const float& speed)
{
    if (App->input->GetKey(SDL_SCANCODE_W) == KeyState::REPEAT) newPos += camera->GetFront() * speed;
    if (App->input->GetKey(SDL_SCANCODE_S) == KeyState::REPEAT) newPos -= camera->GetFront() * speed;

    if (App->input->GetKey(SDL_SCANCODE_A) == KeyState::REPEAT) newPos -= camera->GetRight() * speed;
    if (App->input->GetKey(SDL_SCANCODE_D) == KeyState::REPEAT) newPos += camera->GetRight() * speed;

    if (App->input->GetKey(SDL_SCANCODE_E) == KeyState::REPEAT) newPos += camera->GetUp() * speed;
    if (App->input->GetKey(SDL_SCANCODE_Q) == KeyState::REPEAT) newPos -= camera->GetUp() * speed;
}

void ModuleCamera3D::HandleCameraRotation(const float& sensitivity, const float& dt)
{
    int dx = -App->input->GetMouseXMotion();
    int dy = -App->input->GetMouseYMotion();

    float s = sensitivity * dt;

    if (dx != 0)
    {
        float deltaX = static_cast<float>(dx) * s;
        glm::vec3 rotationAxis(0.0f, 1.0f, 0.0f);

        // GLM quaternion rotation
        glm::quat rotationQuat = glm::angleAxis(deltaX, rotationAxis);

        camera->SetUp(glm::normalize(rotationQuat * camera->GetUp()));
        camera->SetFront(glm::normalize(rotationQuat * camera->GetFront()));
    }

    if (dy != 0)
    {
        float deltaY = static_cast<float>(dy) * s;
        glm::quat rotationQuat = glm::angleAxis(deltaY, camera->GetRight());

        camera->SetUp(glm::normalize(rotationQuat * camera->GetUp()));
        camera->SetFront(glm::normalize(rotationQuat * camera->GetFront()));

        // Prevent flipping
        if (camera->GetUp().y < 0.0f)
        {
            glm::vec3 front = camera->GetFront();
            front.y = std::clamp(front.y, -0.95f, 0.95f);
            front = glm::normalize(front);
            camera->SetFront(front);

            // GLM cross-product
            camera->SetUp(glm::normalize(glm::cross(front, camera->GetRight())));
        }
    }
}

void ModuleCamera3D::HandleCameraZoom(glm::vec3& newPos, const float& speed)
{
    int mouseZ = App->input->GetMouseZ();
    if (mouseZ > 0) newPos += camera->GetFront() * speed;
    if (mouseZ < 0) newPos -= camera->GetFront() * speed;
}

void ModuleCamera3D::HandleCameraPan(glm::vec3& newPos, const float& speed, const float& sensitivity, const float& dt)
{
    int dx = -App->input->GetMouseXMotion();
    int dy = -App->input->GetMouseYMotion();

    float s = sensitivity * dt;
    float deltaX = static_cast<float>(dx) * s;
    float deltaY = static_cast<float>(dy) * s;

    newPos -= camera->GetUp() * speed * deltaY;
    newPos += camera->GetRight() * speed * deltaX;
}

void ModuleCamera3D::HandleCameraOrbit(const float& sensitivity, const float& dt, const glm::vec3& lookAt)
{
    float distToRef = glm::length(lookAt - camera->GetPos());
    HandleCameraRotation(sensitivity, dt);
    camera->SetPos(lookAt + camera->GetFront() * (-distToRef));
}