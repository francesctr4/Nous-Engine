#pragma once

#include "Engine/Modules/Module.h"
#include "Engine/EngineExport.h"
#include "Engine/Core/Globals.h"
#include "Engine/Modules/ModuleScene/include/SceneRenderData.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include <string>
#include <vector>
#include <atomic>
#include "Engine/Core/EventSystem/IEventListener.h"

class Scene;
class Camera;
class ScriptManager;

// Dependency Injection
class ModuleInput;
class ModuleResourceManager;
class ModuleAudio;

enum class SimulationState : uint8_t { STOPPED, PLAYING, PAUSED };

class ModuleScene : public Module, public IEventListener
{
public:

	// Constructor
	ModuleScene(EventSystem* eventSystem, nous::engine::multithreading::NOUS_JobSystem* jobSystem,
		ModuleInput* moduleInput, ModuleResourceManager* moduleResourceManager,
		ModuleAudio* moduleAudio);

	// Destructor
	~ModuleScene() override;

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

	// Instantiates a .nprefab file into the active scene, optionally under parent.
	// Returns the root of the instantiated prefab (null handle on failure).
	NOUS_ENGINE_API GameObject InstantiatePrefab(const std::string& path, GameObject parent = {}) const;

	// Creates a root GameObject + one child GO per submesh for the given mesh
	// asset.  Each child has CTransform (from the node's local transform),
	// CMesh (individual submesh resource), and default CMaterial.
	// Safe to call from a job thread via CreateGameObjectDetached + RegisterGameObject.
	NOUS_ENGINE_API void SpawnMeshAsHierarchy(const std::string& assetsPath) const;

	// ---------------------------------------------------------------------------
	// Simulation controls
	// ---------------------------------------------------------------------------
	NOUS_ENGINE_API void SetSnapshotEnabled(bool enabled) { m_snapshotEnabled = enabled; }

	NOUS_ENGINE_API void RecompileScripts() const;

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

	// Returns true if the active scene contains at least one CCamera with isMainCamera=true.
	NOUS_ENGINE_API bool HasMainCamera() const;

	// True while a LoadSceneAsync job is in flight. Read by ModuleRenderer3D to skip
	// per-entity iteration that would otherwise race the worker thread mutating the registry.
	NOUS_ENGINE_API bool IsLoadingScene() const { return m_isLoadingScene.load(std::memory_order_acquire); }

	// Returns the aspect ratio of the current window, kept up-to-date by WINDOW_RESIZED events.
	NOUS_ENGINE_API float GetWindowAspect() const { return m_windowAspect; }

	// Broker accessor: components (CAudioSource) reach the audio module through the
	// scene, mirroring how CScript reaches scriptManager. May be null in tests.
	NOUS_ENGINE_API ModuleAudio* GetAudio() const { return mModuleAudio; }

	NOUS_ENGINE_API bool IsSelected(GameObject go) const;
	NOUS_ENGINE_API void AddToSelection(GameObject go);      // no-op if already in set; updates primarySelection
	NOUS_ENGINE_API void RemoveFromSelection(GameObject go); // primarySelection = back() or {} after remove
	NOUS_ENGINE_API void SetSelection(GameObject go);        // clears set, adds go, sets primarySelection
	NOUS_ENGINE_API void ClearSelection();                   // empties both fields

public:

	Scene*         activeScene    = nullptr;
	std::vector<GameObject> selectedGameObjects;  // ordered by selection time; empty = nothing selected
	GameObject              primarySelection;      // last item added; invalid handle when set is empty
	Camera*        gameCamera    = nullptr;
	ScriptManager* scriptManager = nullptr;
	// Set by GameViewport::Draw() each frame; applied in ModuleRenderer3D::PostUpdate()
	// before frustum build and DrawFrame() to override CCamera's authored aspect ratio.
	// 0.0f = not yet set (first frame before the viewport has drawn).
	float          gameViewportAspect = 0.0f;

	// Last window aspect ratio received via WINDOW_RESIZED. Initialised to the compile-time
	// default window size and updated on every resize to keep CCamera components in sync.
	float          m_windowAspect = static_cast<float>(WINDOW_WIDTH) / static_cast<float>(WINDOW_HEIGHT);

private:

	// Dependency Injection
	ModuleInput* mModuleInput;
	ModuleResourceManager* mModuleResourceManager;
	ModuleAudio* mModuleAudio;

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
	void RefreshPrefabInstances() const;
};