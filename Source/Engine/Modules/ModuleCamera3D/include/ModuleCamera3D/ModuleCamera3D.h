#ifndef MODULE_CAMERA_3D_H
#define MODULE_CAMERA_3D_H

#include <ModuleBase/Module.h>
#include <EventSystem/IEventListener.h>
#include <EngineCore/IInputReader.h>

#include <glm/glm.hpp>

class Camera;

class ModuleCamera3D : public Module, public IEventListener
{
public:

	explicit ModuleCamera3D(EventSystem* eventSystem, nous::engine::multithreading::NOUS_JobSystem* jobSystem, IInputReader* moduleInput);
	~ModuleCamera3D() override;

	bool Awake() override;
	bool Start() override;

	UpdateStatus PreUpdate(float dt) override;
	UpdateStatus Update(float dt) override;
	UpdateStatus PostUpdate(float dt) override;

	bool CleanUp() override;

	void OnEvent(const Event& event) override;

	NOUS_ENGINE_API Camera* GetCamera();

	void SetOrbitTarget(const glm::vec3& worldPos) { m_orbitTarget = worldPos; m_hasOrbitTarget = true; }
	void ClearOrbitTarget()                         { m_hasOrbitTarget = false; }
	NOUS_ENGINE_API void FrameTarget(const glm::vec3& target, float distance = 5.0f);

	bool sceneViewportHovered;

private:

	void HandleCameraMovement(glm::vec3& newPos, const float& speed);
	void HandleCameraRotation(const float& sensitivity);
	void HandleCameraZoom(glm::vec3& newPos, const float& speed);
	void HandleCameraPan(glm::vec3& newPos, const float& sensitivity);
	void HandleCameraOrbit(const float& sensitivity, const glm::vec3& lookAt);

	Camera* camera;

	glm::vec3 m_orbitTarget  {};
	bool      m_hasOrbitTarget = false;

	// Dependency Injection
	IInputReader* mModuleInput;

};

#endif // MODULE_CAMERA_3D_H