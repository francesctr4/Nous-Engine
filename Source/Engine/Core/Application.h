#ifndef APPLICATION_H
#define APPLICATION_H

#include "Engine/EngineExport.h"
#include "Engine/Core/UpdateStatus.h"

#include <vector>

// Forward declarations
struct Event;
class EventSystem;
class Timer;
namespace NOUS_Multithreading { class NOUS_JobSystem; }

class Module;
class ModuleWindow;
class ModuleInput;
class ModuleCamera3D;
class ModuleResourceManager;
class ModuleScene;
class ModuleRenderer3D;
class IImporterManager;

class Application
{
public:

	NOUS_ENGINE_API explicit Application(bool isGameMode = false);
	NOUS_ENGINE_API ~Application();

	NOUS_ENGINE_API bool Awake() const;
	NOUS_ENGINE_API bool Start() const;
	NOUS_ENGINE_API UpdateStatus Update();
	NOUS_ENGINE_API bool CleanUp() const;

	NOUS_ENGINE_API void SetTargetFPS(float FPS);
	NOUS_ENGINE_API float GetTargetFPS() const;

	NOUS_ENGINE_API bool IsGameMode() const;

	NOUS_ENGINE_API float GetFPS() const;
	NOUS_ENGINE_API float GetDT() const;
	NOUS_ENGINE_API float GetMS() const;

    // Event System
    NOUS_ENGINE_API void        QueueEvent(const Event& event)    const;
    NOUS_ENGINE_API void        BroadcastEvent(const Event& event) const;
    NOUS_ENGINE_API EventSystem* GetEventSystem()                  const;

    // ------------- MODULE ACCESSORS ------------- //
    NOUS_ENGINE_API ModuleWindow*                         GetWindow()          const;
    NOUS_ENGINE_API ModuleInput*                          GetInput()           const;
    NOUS_ENGINE_API ModuleCamera3D*                       GetCamera()          const;
    NOUS_ENGINE_API ModuleResourceManager*                GetResourceManager() const;
    NOUS_ENGINE_API ModuleScene*                          GetScene()           const;
    NOUS_ENGINE_API ModuleRenderer3D*                     GetRenderer()        const;

	// ------------- MULTITHREADING ------------- //
    NOUS_ENGINE_API NOUS_Multithreading::NOUS_JobSystem*  GetJobSystem()       const;


private:

	UpdateStatus PrepareUpdate();
	void FinishUpdate() const;

	ModuleWindow*          window;
	ModuleInput*           input;
	ModuleCamera3D*        camera;
	ModuleResourceManager* resourceManager;
	ModuleScene*           scene;
	ModuleRenderer3D*      renderer;

	bool m_isGameMode;

	EventSystem*                         eventSystem;
	NOUS_Multithreading::NOUS_JobSystem* jobSystem;
	IImporterManager*                    importerManager;

	std::vector<Module*> listModules;

	Timer* msTimer;
	float  dt;
	float  targetFPS;

	Timer* updateTitleTimer;

	mutable float cachedDt        = 0.0f;
	mutable float cachedFPS       = 0.0f;
	mutable char  titleBuffer[256] = {};
};

#endif // APPLICATION_H