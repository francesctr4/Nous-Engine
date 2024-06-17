#pragma once

#include "Globals.h"

#define NUM_MODULES 2

class Module;
class ModuleWindow;
class ModuleInput;

class Application
{
public:

	ModuleWindow* window;
	ModuleInput* input;

private:

	Module* list_modules[NUM_MODULES];

public:

	Application();
	~Application();

	bool Awake();
	UpdateStatus Update();
	bool CleanUp();

private:

	void AddModule(Module* mod);
	void PrepareUpdate();
	void FinishUpdate();
};

extern Application* External;