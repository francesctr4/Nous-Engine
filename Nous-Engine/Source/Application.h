#pragma once

#include "Globals.h"
#include "EventSystem.h"
#include "Timer.h"

#include "NOUS_JobSystem.h"

constexpr uint16 NUM_MODULES = 8;

class Module;
class ModuleWindow;
class ModuleInput;
class ModuleFileSystem;
class ModuleCamera3D;
class ModuleResourceManager;
class ModuleScene;
class ModuleRenderer3D;
class ModuleEditor;

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
	ModuleFileSystem* fileSystem;
	ModuleCamera3D* camera;
	ModuleResourceManager* resourceManager;
	ModuleScene* scene;
	ModuleRenderer3D* renderer;
	ModuleEditor* editor;

	bool isMinimized;

	// ------------- MULTITHREADING ------------- //
	NOUS_Multithreading::NOUS_JobSystem* jobSystem;
	
private:

	UpdateStatus PrepareUpdate();
	void FinishUpdate();

	// ---------------------------------------- \\

	Module* listModules[NUM_MODULES];

	Timer msTimer;
	float dt;
	float targetFPS;

	Timer updateTitleTimer;
};

extern Application* External;