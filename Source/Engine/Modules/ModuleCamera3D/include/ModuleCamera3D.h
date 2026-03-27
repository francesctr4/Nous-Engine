#ifndef MODULE_CAMERA_3D_H
#define MODULE_CAMERA_3D_H

#include "Engine/Modules/Module.h"
#include "Engine/Core/EventSystem/IEventListener.h"

#include <glm/glm.hpp>

class Camera;

// Dependency Injection
class ModuleInput;

class ModuleCamera3D : public Module, public IEventListener
{
public:

	explicit ModuleCamera3D(EventSystem* eventSystem, NOUS_Multithreading::NOUS_JobSystem* jobSystem, ModuleInput* moduleInput);
	~ModuleCamera3D() override;

	bool Awake() override;
	bool Start() override;

	UpdateStatus PreUpdate(float dt) override;
	UpdateStatus Update(float dt) override;
	UpdateStatus PostUpdate(float dt) override;

	bool CleanUp() override;

	void OnEvent(const Event& event) override;

	NOUS_ENGINE_API Camera* GetCamera();

	bool sceneViewportHovered;

private:

	void HandleCameraMovement(glm::vec3& newPos, const float& speed);
	void HandleCameraRotation(const float& sensitivity, const float& dt);
	void HandleCameraZoom(glm::vec3& newPos, const float& speed);
	void HandleCameraPan(glm::vec3& newPos, const float& speed, const float& sensitivity, const float& dt);
	void HandleCameraOrbit(const float& sensitivity, const float& dt, const glm::vec3& lookAt);

	Camera* camera;

	// Dependency Injection
	ModuleInput* mModuleInput;

};

#endif // MODULE_CAMERA_3D_H