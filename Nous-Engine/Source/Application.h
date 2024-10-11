#pragma once

#include "Globals.h"
#include "EventSystem.h"

#define NUM_MODULES 4

class Module;
class ModuleWindow;
class ModuleInput;
class ModuleCamera3D;
class ModuleRenderer3D;

class Application
{
public:

	Application();
	~Application();

	bool Awake();
	UpdateStatus Update();
	bool CleanUp();

	void BroadcastEvent(const Event& event);

	// ---------------------------------------- \\

	ModuleWindow* window;
	ModuleInput* input;
	ModuleCamera3D* camera;
	ModuleRenderer3D* renderer;
	
private:

	UpdateStatus PrepareUpdate();
	void FinishUpdate();

	// ---------------------------------------- \\

	Module* list_modules[NUM_MODULES];
	float targetFPS;

};

extern Application* External;