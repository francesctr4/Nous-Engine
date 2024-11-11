#pragma once

#include "Globals.h"
#include "EventSystem.h"
#include "Timer.h"

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

	void SetTargetFPS(float FPS);
	float GetTargetFPS();

	float GetFPS();
	float GetDT();
	float GetMS();

	// ---------------------------------------- \\

	ModuleWindow* window;
	ModuleInput* input;
	ModuleCamera3D* camera;
	ModuleRenderer3D* renderer;

	bool isMinimized;
	
private:

	UpdateStatus PrepareUpdate();
	void FinishUpdate();

	// ---------------------------------------- \\

	Module* list_modules[NUM_MODULES];

	Timer msTimer;
	float dt;
	float targetFPS;

};

extern Application* External;