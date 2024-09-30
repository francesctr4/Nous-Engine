#pragma once

#include "Globals.h"
#include "ThreadPool.h"

#define NUM_MODULES 4

class Module;
class ModuleWindow;
class ModuleInput;
class ModuleCamera3D;
class ModuleRenderer3D;

// Barrier to synchronize all threads at the end of each phase
std::barrier syncPoint(NUM_MODULES, []() noexcept {
	// This completion function is called once all threads reach the barrier
	// You can use this to check for exit conditions after each phase if needed.
	NOUS_INFO("All modules reached the barrier, moving to the next phase...");

	});

class Application
{
public:

	Application();
	~Application();

	bool Awake();
	UpdateStatus Update();
	bool CleanUp();

	// ---------------------------------------- \\

	ModuleWindow* window;
	ModuleInput* input;
	ModuleCamera3D* camera;
	ModuleRenderer3D* renderer;

	ThreadPool* threadPool;
	
private:

	UpdateStatus PrepareUpdate();
	void FinishUpdate();

	// ---------------------------------------- \\

	Module* list_modules[NUM_MODULES];
	float targetFPS;

};

extern Application* External;