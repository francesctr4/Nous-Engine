#pragma once

#include "Globals.h"

#define NUM_MODULES 4

class Module;
class ModuleWindow;
class ModuleInput;
class ModuleCamera3D;
class ModuleRenderer3D;

class Application
{
public:

	ModuleWindow* window;
	ModuleInput* input;
	ModuleCamera3D* camera;
	ModuleRenderer3D* renderer;
	
private:

	Module* list_modules[NUM_MODULES];

	float targetFPS;

public:

	Application();
	~Application();

	bool Awake();
	UpdateStatus Update();
	bool CleanUp();

private:

	UpdateStatus PrepareUpdate();
	void FinishUpdate();

};

extern Application* External;