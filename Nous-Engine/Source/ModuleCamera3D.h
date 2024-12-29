#pragma once

#include "Module.h"
#include "Camera.h"

class ModuleCamera3D : public Module
{
public:

	ModuleCamera3D(Application* app, std::string name, bool start_enabled = true);
	virtual ~ModuleCamera3D();

	bool Awake() override;
	bool Start() override;

	UpdateStatus PreUpdate(float dt) override;
	UpdateStatus Update(float dt) override;
	UpdateStatus PostUpdate(float dt) override;

	bool CleanUp() override;

	void ReceiveEvent(const Event& event) override;

	Camera* GetCamera();

private:

	void HandleCameraMovement(float3& newPos, const float& speed);
	void HandleCameraRotation(const float& sensitivity, const float& dt);
	void HandleCameraZoom(float3& newPos, const float& speed);
	void HandleCameraPan(float3& newPos, const float& speed, const float& sensitivity, const float& dt);
	void HandleCameraOrbit(float3& newPos, const float& sensitivity, const float& dt, const float3& lookAt);

private:

	Camera camera;

};