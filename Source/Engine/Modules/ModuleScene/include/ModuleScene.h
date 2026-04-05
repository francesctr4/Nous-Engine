#ifndef MODULESCENE_H
#define MODULESCENE_H

#include "Engine/Modules/Module.h"
#include "Engine/EngineExport.h"
#include "Engine/Modules/ModuleScene/include/SceneRenderData.h"
#include <string>
#include <vector>
#include <atomic>
#include "Engine/Core/EventSystem/IEventListener.h"
#include "Engine/Modules/ModuleResourceManager/include/ModuleResourceManager.h"

class Scene;
class GameObject;
class Camera;
class ScriptManager;

// Dependency Injection
class ModuleInput;
class ModuleResourceManager;

enum class SimulationState { STOPPED, PLAYING, PAUSED };

class ModuleScene : public Module, public IEventListener
{
public:

	// Constructor
	ModuleScene(EventSystem* eventSystem, NOUS_Multithreading::NOUS_JobSystem* jobSystem,
		ModuleInput* moduleInput, ModuleResourceManager* moduleResourceManager);

	// Destructor
	virtual ~ModuleScene();

	bool Awake() override;
	bool Start() override;

	UpdateStatus PreUpdate(float dt) override;
	UpdateStatus Update(float dt) override;
	UpdateStatus PostUpdate(float dt) override;

	bool CleanUp() override;

	void OnEvent(const Event& event) override;

	NOUS_ENGINE_API void SaveScene(const std::string& path);
	NOUS_ENGINE_API void LoadScene(const std::string& path);
	NOUS_ENGINE_API void LoadSceneAsync(const std::string& path);
	NOUS_ENGINE_API void ClearScene();

	// Clears the active scene and resets its tracked path so the next save
	// will require a target path (i.e. Save As...).
	NOUS_ENGINE_API void NewScene(const std::string& name = "Untitled Scene");

	// Returns the path of the scene currently loaded/saved, or empty string
	// if the active scene has never been persisted.
	NOUS_ENGINE_API const std::string& GetCurrentScenePath() const { return m_currentScenePath; }
	NOUS_ENGINE_API bool                HasCurrentScenePath() const { return !m_currentScenePath.empty(); }

	// Instantiates a .nprefab file into the active scene, optionally under parentGO.
	// Returns the root of the instantiated prefab, or nullptr on failure.
	NOUS_ENGINE_API GameObject* InstantiatePrefab(const std::string& path, GameObject* parentGO = nullptr);

	// Creates a root GameObject + one child GO per submesh for the given mesh
	// asset.  Each child has CTransform (from the node's local transform),
	// CMesh (individual submesh resource), and default CMaterial.
	// Safe to call from a job thread via CreateGameObjectDetached + RegisterGameObject.
	NOUS_ENGINE_API void SpawnMeshAsHierarchy(const std::string& assetsPath);

	// ---------------------------------------------------------------------------
	// Simulation controls
	// ---------------------------------------------------------------------------
	NOUS_ENGINE_API void SetSnapshotEnabled(bool enabled) { m_snapshotEnabled = enabled; }

	NOUS_ENGINE_API void RecompileScripts();

	NOUS_ENGINE_API void PressPlay();   // STOPPED → PLAYING
	NOUS_ENGINE_API void PressStop();   // PLAYING/PAUSED → STOPPED (restores scene snapshot)
	NOUS_ENGINE_API void PressPause();  // PLAYING ↔ PAUSED toggle
	NOUS_ENGINE_API void PressStep();   // Advances exactly one frame while PAUSED

	NOUS_ENGINE_API bool            IsPlaying()           const { return m_simulationState == SimulationState::PLAYING; }
	NOUS_ENGINE_API bool            IsPaused()            const { return m_simulationState == SimulationState::PAUSED;  }
	NOUS_ENGINE_API bool            IsStopped()           const { return m_simulationState == SimulationState::STOPPED; }
	NOUS_ENGINE_API SimulationState GetSimulationState()  const { return m_simulationState; }

	// Returns the per-frame snapshot built in PostUpdate for consumption by the renderer.
	NOUS_ENGINE_API const SceneRenderData& GetRenderData() const { return m_renderData; }

public:

	Scene*         activeScene    = nullptr;
	GameObject*    selectedGameObject = nullptr;
	Camera*        gameCamera    = nullptr;
	ScriptManager* scriptManager = nullptr;

private:

	// Dependency Injection
	ModuleInput* mModuleInput;
	ModuleResourceManager* mModuleResourceManager;

	bool m_snapshotEnabled = false;

	SceneRenderData m_renderData;

	// ---------------------------------------------------------------------------
	// Simulation state
	// ---------------------------------------------------------------------------
	SimulationState m_simulationState  = SimulationState::STOPPED;
	bool            m_stepOneFrame     = false;  // consume in Update() to tick one frame while PAUSED
	bool            m_didStepThisFrame = false;  // propagates to PostUpdate() for LateUpdate gating
	std::atomic<bool> m_pendingStop          = false;  // deferred LoadScene flag — set by PressStop(), consumed in PreUpdate()
	std::atomic<bool> m_pendingPrefabRefresh = false;  // set by LoadSceneAsync job, consumed in PreUpdate() on main thread
	std::atomic<bool> m_isLoadingScene       = false;  // true while a LoadSceneAsync job is in flight; guards re-entrancy
	std::string     m_snapshotPath     = "Library/_simulation_snapshot.nous";
	std::string     m_currentScenePath;  // empty = unsaved; set by LoadScene/SaveScene

	// After a scene is deserialized, re-instantiates any GO that carries a CPrefab
	// component from its source .nprefab file, replacing stale inline children.
	void RefreshPrefabInstances();

	/**
	 * @brief Ensures at least one GameObject with a main CCamera exists in the
	 *        active scene. If none is found, a "Main Camera" GO is created and
	 *        positioned at the current gameCamera location.
	 *
	 * Called at the end of every LoadScene job so that scenes saved before the
	 * CCamera system existed automatically get a camera on load.
	 */
	void EnsureMainCamera();
};

#endif // MODULESCENE_H