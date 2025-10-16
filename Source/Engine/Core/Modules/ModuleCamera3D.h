#ifndef MODULE_CAMERA_3D_H
#define MODULE_CAMERA_3D_H

#include <Engine/Core/Module.h>
#include <Engine/Systems/Camera System/Camera.h>

#include <glm/glm.hpp>
#include "Engine/Systems/Event System/IEventListener.h"

class ModuleCamera3D : public Module, public IEventListener
{
public:

	ModuleCamera3D(Application* app);
	virtual ~ModuleCamera3D();

	bool Awake() override;
	bool Start() override;

	UpdateStatus PreUpdate(float dt) override;
	UpdateStatus Update(float dt) override;
	UpdateStatus PostUpdate(float dt) override;

	bool CleanUp() override;

	void OnEvent(const Event& event) override;

	Camera* GetCamera();

	bool sceneViewportHovered;

private:

	void HandleCameraMovement(glm::vec3& newPos, const float& speed);
	void HandleCameraRotation(const float& sensitivity, const float& dt);
	void HandleCameraZoom(glm::vec3& newPos, const float& speed);
	void HandleCameraPan(glm::vec3& newPos, const float& speed, const float& sensitivity, const float& dt);
	void HandleCameraOrbit(const float& sensitivity, const float& dt, const glm::vec3& lookAt);

private:

	Camera camera;

};

#endif // MODULE_CAMERA_3D_H