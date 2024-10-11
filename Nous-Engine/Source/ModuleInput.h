#pragma once

#include "Module.h"
#include "SDL2.h"
#include "Globals.h"

class ModuleInput : public Module 
{
public: 

	ModuleInput(Application* app, std::string name, bool start_enabled = true);
	virtual ~ModuleInput();

	bool Awake() override;
	bool Start() override;
	UpdateStatus PreUpdate(float dt) override;
	bool CleanUp() override;

	void ReceiveEvent(const Event& event) override;

};