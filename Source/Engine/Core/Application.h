#ifndef APPLICATION_H
#define APPLICATION_H

#include <Engine/Core/EngineExport.h>
#include <Engine/Core/UpdateStatus.h>
#include <vector>

// Forward declarations
struct Event;
class EventSystem;
class Timer;
namespace NOUS_Multithreading { class NOUS_JobSystem; }

constexpr uint8_t NUM_MODULES = 7;

class Module;
class ModuleWindow;
class ModuleInput;
class ModuleFileSystem;
class ModuleCamera3D;
class ModuleResourceManager;
class ModuleScene;
class ModuleRenderer3D;

class Application
{
public:

	NOUS_ENGINE_API Application();
	NOUS_ENGINE_API ~Application();

	NOUS_ENGINE_API bool Awake();
	NOUS_ENGINE_API bool Start();
	NOUS_ENGINE_API UpdateStatus Update();
	NOUS_ENGINE_API bool CleanUp();

	NOUS_ENGINE_API void SetTargetFPS(float FPS);
	NOUS_ENGINE_API float GetTargetFPS();

	NOUS_ENGINE_API float GetFPS();
	NOUS_ENGINE_API float GetDT();
	NOUS_ENGINE_API float GetMS();

    // Event System
    NOUS_ENGINE_API void QueueEvent(const Event& event);
    NOUS_ENGINE_API void BroadcastEvent(const Event& event);

private:

	UpdateStatus PrepareUpdate();
	void FinishUpdate();

public:

	ModuleWindow* window;
	ModuleInput* input;
	ModuleFileSystem* fileSystem;
	ModuleCamera3D* camera;
	ModuleResourceManager* resourceManager;
	ModuleScene* scene;
	ModuleRenderer3D* renderer;

	bool isMinimized;

    // ------------- EVENT SYSTEM ------------- //
    EventSystem* eventSystem;

	// ------------- MULTITHREADING ------------- //
	NOUS_Multithreading::NOUS_JobSystem* jobSystem;

private:

	Module* listModules[NUM_MODULES];

	Timer* msTimer;
	float dt;
	float targetFPS;

	Timer* updateTitleTimer;
};

extern NOUS_ENGINE_API Application* External;

#endif // APPLICATION_H