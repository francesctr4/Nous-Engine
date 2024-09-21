#pragma once

#include "Module.h"

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

};