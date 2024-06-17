#pragma once

#include "Module.h"
#include "SDL2.h"

class ModuleInput : public Module 
{
public: 

	ModuleInput(Application* app, bool start_enabled = true);
	virtual ~ModuleInput();

	bool Awake() override;
	bool Start() override;
	UpdateStatus PreUpdate(float dt) override;
	bool CleanUp() override;

};