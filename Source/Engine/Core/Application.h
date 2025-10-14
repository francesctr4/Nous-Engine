#ifndef APPLICATION_H
#define APPLICATION_H

#include <Engine/Core/Export.h>
#include <Engine/Core/UpdateStatus.h>

// Forward declarations
struct Event;
class Timer;
namespace NOUS_Multithreading { class NOUS_JobSystem; }

constexpr uint8_t NUM_MODULES = 8;

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

	NOUS_API Application();
	NOUS_API ~Application();

	NOUS_API bool Awake();
	NOUS_API UpdateStatus Update();
	NOUS_API bool CleanUp();

	NOUS_API void BroadcastEvent(const Event& event);

	NOUS_API void SetTargetFPS(float FPS);
	NOUS_API float GetTargetFPS();

	NOUS_API float GetFPS();
	NOUS_API float GetDT();
	NOUS_API float GetMS();

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
	ModuleEditor* editor;

	bool isMinimized;

	// ------------- MULTITHREADING ------------- //
	NOUS_Multithreading::NOUS_JobSystem* jobSystem;

private:

	Module* listModules[NUM_MODULES];

	Timer* msTimer;
	float dt;
	float targetFPS;

	Timer* updateTitleTimer;
};

extern NOUS_API Application* External;

#endif // APPLICATION_H