#pragma once

#include "Module.h"

#include "DynamicArray.h"

class ModuleRenderer3D : public Module
{
public:

	ModuleRenderer3D(Application* app, std::string name, bool start_enabled = true);
	virtual ~ModuleRenderer3D();

	bool Awake() override;
	bool Start() override;

	UpdateStatus PreUpdate(float dt) override;
	UpdateStatus Update(float dt) override;
	UpdateStatus PostUpdate(float dt) override;

	bool CleanUp() override;

	DynamicArray<uint64> mydarray;

};