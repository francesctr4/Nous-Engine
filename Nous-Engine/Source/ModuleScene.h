#pragma once

#include "Module.h"

class Camera;

class ModuleScene : public Module
{
public:

	// Constructor
	ModuleScene(Application* app, std::string name, bool start_enabled = true);

	// Destructor
	virtual ~ModuleScene();

	bool Awake() override;
	bool Start() override;

	UpdateStatus PreUpdate(float dt) override;
	UpdateStatus Update(float dt) override;
	UpdateStatus PostUpdate(float dt) override;

	bool CleanUp() override;

	void ReceiveEvent(const Event& event) override;

public:

	Camera* gameCamera;

};