#ifndef APPLICATION_H
#define APPLICATION_H

#include "Engine/EngineExport.h"
#include "Engine/Core/UpdateStatus.h"

#include <array>

// Forward declarations
struct Event;
class EventSystem;
class Timer;
namespace NOUS_Multithreading { class NOUS_JobSystem; }

constexpr uint8_t NUM_MODULES = 6;

class Module;
class ModuleWindow;
class ModuleInput;
class ModuleCamera3D;
class ModuleResourceManager;
class ModuleScene;
class ModuleRenderer3D;

class Application
{
public:

	NOUS_ENGINE_API Application();
	NOUS_ENGINE_API ~Application();

	NOUS_ENGINE_API bool Awake() const;
	NOUS_ENGINE_API bool Start() const;
	NOUS_ENGINE_API UpdateStatus Update();
	NOUS_ENGINE_API bool CleanUp();

	NOUS_ENGINE_API void SetTargetFPS(float FPS);
	NOUS_ENGINE_API float GetTargetFPS() const;

	NOUS_ENGINE_API float GetFPS() const;
	NOUS_ENGINE_API float GetDT() const;
	NOUS_ENGINE_API float GetMS() const;

    // Event System
    NOUS_ENGINE_API void QueueEvent(const Event& event) const;
    NOUS_ENGINE_API void BroadcastEvent(const Event& event) const;

private:

	UpdateStatus PrepareUpdate();
	void FinishUpdate() const;

public:

	ModuleWindow* window;
	ModuleInput* input;
	ModuleCamera3D* camera;
	ModuleResourceManager* resourceManager;
	ModuleScene* scene;
	ModuleRenderer3D* renderer;

	bool isMinimized;
	bool isGameMode;

	NOUS_ENGINE_API void SetGameMode(bool gameMode) { isGameMode = gameMode; }
	[[nodiscard]] NOUS_ENGINE_API bool IsGameMode() const { return isGameMode; }

    // ------------- EVENT SYSTEM ------------- //
    EventSystem* eventSystem;

	// ------------- MULTITHREADING ------------- //
	NOUS_Multithreading::NOUS_JobSystem* jobSystem;

private:

	std::array<Module*, NUM_MODULES> listModules;

	Timer* msTimer;
	float dt;
	float targetFPS;

	Timer* updateTitleTimer;
};

extern NOUS_ENGINE_API Application* External;

#endif // APPLICATION_H