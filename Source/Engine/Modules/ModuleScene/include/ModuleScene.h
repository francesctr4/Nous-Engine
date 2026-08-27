#pragma once

#include "Engine/Modules/Module.h"
#include "Engine/EngineExport.h"
#include "Engine/Core/Globals.h"
#include "Engine/Modules/ModuleScene/include/SceneRenderData.h"
#include <ECS/GameObject.h>
#include <ECS/Scene/iSceneHost.h>
#include <ECS/ComponentServices.h>
#include "Engine/Scripting/iScriptSceneHost.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>
#include <atomic>
#include "Engine/Core/EventSystem/IEventListener.h"

class Scene;
class Camera;
class ScriptManager;
class IScriptRegistry;
class ResourceMesh;
class ResourceMaterial;

// Dependency Injection
class ModuleInput;
class ModuleResourceManager;
class ModuleAudio;
class ModuleVideo;

enum class SimulationState : uint8_t { STOPPED, PLAYING, PAUSED };

// Plain-data description of a model to be turned into GameObjects. Produced on a
// worker thread (file I/O, resource resolution, matrix decomposition) and consumed
// on the main thread by BuildModelHierarchy. Deliberately holds no entt types and
// no file handles: everything slow has already happened by the time this exists.
struct PendingSubMesh
{
	std::string       name;
	glm::vec3         position{ 0.0f };
	glm::quat         orientation{ 1.0f, 0.0f, 0.0f, 0.0f };
	glm::vec3         scale{ 1.0f };
	ResourceMesh*     mesh         = nullptr;
	ResourceMaterial* material     = nullptr;   // already resolved (default included)
	int32_t           submeshIndex = 0;
};

struct PendingModelSpawn
{
	std::string                 rootName;
	std::vector<PendingSubMesh> submeshes;
};

/// @brief Turns a loaded model description into GameObjects in the given scene.
/// @note MAIN THREAD ONLY - it mutates the entt registry.
/// @note A free function rather than a ModuleScene method so it can be unit-tested
///       without constructing a module (which would drag in the ResourceManager and
///       the JobSystem). ModuleScene::BuildModelHierarchy forwards to it.
NOUS_ENGINE_API void BuildModelHierarchyInto(Scene& scene, PendingModelSpawn&& spawn);

class ModuleScene : public Module, public IEventListener, public ISceneHost,
                    public IScriptSceneHost
{
public:

	// Constructor
	ModuleScene(EventSystem* eventSystem, nous::engine::multithreading::NOUS_JobSystem* jobSystem,
		ModuleInput* moduleInput, ModuleResourceManager* moduleResourceManager,
		ModuleAudio* moduleAudio, ModuleVideo* moduleVideo);

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
	NOUS_ENGINE_API void LoadSceneAsync(const std::string& path) override;
	NOUS_ENGINE_API void ClearScene();

	// Clears the active scene and resets its tracked path so the next save
	// will require a target path (i.e. Save As...).
	NOUS_ENGINE_API void NewScene(const std::string& name = "Untitled Scene");

	// Returns the path of the scene currently loaded/saved, or empty string
	// if the active scene has never been persisted.
	NOUS_ENGINE_API const std::string& GetCurrentScenePath() const override { return m_currentScenePath; }
	NOUS_ENGINE_API bool                HasCurrentScenePath() const override { return !m_currentScenePath.empty(); }

	// IScriptSceneHost accessor for the public `activeScene` member below. The
	// member stays for the editor and the other modules that already use it;
	// this is how it crosses the interface into Scripting/.
	[[nodiscard]] NOUS_ENGINE_API Scene* GetActiveScene() const override { return activeScene; }

	// Instantiates a .nprefab file into the active scene, optionally under parent.
	// Returns the root of the instantiated prefab (null handle on failure).
	NOUS_ENGINE_API GameObject InstantiatePrefab(const std::string& path, GameObject parent = {}) const;

	// Creates a root GameObject + one child GO per submesh for the given mesh
	// asset.  Each child has CTransform (from the node's local transform),
	// CMesh (individual submesh resource), and default CMaterial.
	// Safe to call from a job thread via CreateGameObjectDetached + RegisterGameObject.
	NOUS_ENGINE_API void SpawnMeshAsHierarchy(const std::string& assetsPath) const;

	// Main-thread half of SpawnMeshAsHierarchy: turns the plain-data description
	// produced on a worker into GameObjects in the active scene.
	NOUS_ENGINE_API void BuildModelHierarchy(PendingModelSpawn&& spawn);

	// ---------------------------------------------------------------------------
	// Simulation controls
	// ---------------------------------------------------------------------------
	NOUS_ENGINE_API void SetSnapshotEnabled(bool enabled) { m_snapshotEnabled = enabled; }

	NOUS_ENGINE_API void RecompileScripts() const;

	NOUS_ENGINE_API void PressPlay();   // STOPPED → PLAYING
	NOUS_ENGINE_API void PressStop();   // PLAYING/PAUSED → STOPPED (restores scene snapshot)
	NOUS_ENGINE_API void PressPause();  // PLAYING ↔ PAUSED toggle
	NOUS_ENGINE_API void PressStep();   // Advances exactly one frame while PAUSED

	NOUS_ENGINE_API bool            IsPlaying()           const override { return m_simulationState == SimulationState::PLAYING; }
	NOUS_ENGINE_API bool            IsPaused()            const override { return m_simulationState == SimulationState::PAUSED;  }
	NOUS_ENGINE_API bool            IsStopped()           const override { return m_simulationState == SimulationState::STOPPED; }
	NOUS_ENGINE_API SimulationState GetSimulationState()  const { return m_simulationState; }

	// Returns the per-frame snapshot built in PostUpdate for consumption by the renderer.
	NOUS_ENGINE_API const SceneRenderData& GetRenderData() const { return m_renderData; }

	// Returns true if the active scene contains at least one CCamera with isMainCamera=true.
	NOUS_ENGINE_API bool HasMainCamera() const;

	// True while a LoadSceneAsync job is in flight. Read by ModuleRenderer3D to skip
	// per-entity iteration that would otherwise race the worker thread mutating the registry.
	NOUS_ENGINE_API bool IsLoadingScene() const override { return m_isLoadingScene.load(std::memory_order_acquire); }

	// Returns the aspect ratio of the current window, kept up-to-date by WINDOW_RESIZED events.
	NOUS_ENGINE_API float GetWindowAspect() const override { return m_windowAspect; }

	// The camera CCamera's main-camera instance drives. May be null.
	NOUS_ENGINE_API Camera* GetGameCamera() const override { return gameCamera; }

	// Editor-only accessor (AudioGraphEditor / AudioMixerWindow need the concrete
	// module for the bus-mixer surface, which is deliberately off IAudioBroker).
	// Components must NOT use this — they reach audio via Services().audio.
	// GetVideo() was deleted with CVideoPlayer's migration; nothing needed it.
	NOUS_ENGINE_API ModuleAudio* GetAudio() const { return mModuleAudio; }

	// Fills the aggregate every Scene this module creates already points at.
	// Called by Application once the full module graph exists — which is AFTER
	// this module's constructor has already built activeScene. That is safe
	// precisely because Scene stores a POINTER to m_componentServices, not a copy:
	// filling it here updates every live Scene in place. Do not change Scene to
	// take the aggregate by value without also moving scene creation later.
	NOUS_ENGINE_API void SetComponentServices(const ComponentServices& services);

	// The script registry, as the interface Systems/ consumes (used by Application
	// when assembling ComponentServices). Defined out-of-line so this header does
	// not need ScriptManager's definition for the derived-to-base conversion.
	NOUS_ENGINE_API IScriptRegistry* GetScriptRegistry() const;

	// The concrete script manager. The editor needs the full type (script-name
	// enumeration, recompile); Systems/ must use GetScriptRegistry() instead.
	NOUS_ENGINE_API ScriptManager* GetScriptManager() const { return scriptManager; }

	NOUS_ENGINE_API bool IsSelected(GameObject go) const;
	NOUS_ENGINE_API void AddToSelection(GameObject go);      // no-op if already in set; updates primarySelection
	NOUS_ENGINE_API void RemoveFromSelection(GameObject go); // primarySelection = back() or {} after remove
	NOUS_ENGINE_API void SetSelection(GameObject go);        // clears set, adds go, sets primarySelection
	NOUS_ENGINE_API void ClearSelection();                   // empties both fields

public:

	Scene*         activeScene    = nullptr;
	std::vector<GameObject> selectedGameObjects;  // ordered by selection time; empty = nothing selected
	GameObject              primarySelection;      // last item added; invalid handle when set is empty
	// Set by GameViewport::Draw() each frame; applied in ModuleRenderer3D::PostUpdate()
	// before frustum build and DrawFrame() to override CCamera's authored aspect ratio.
	// 0.0f = not yet set (first frame before the viewport has drawn).
	float          gameViewportAspect = 0.0f;

	// Last window aspect ratio received via WINDOW_RESIZED. Initialised to the compile-time
	// default window size and updated on every resize to keep CCamera components in sync.
	float          m_windowAspect = static_cast<float>(WINDOW_WIDTH) / static_cast<float>(WINDOW_HEIGHT);

private:

	// Reached through GetGameCamera() / GetScriptRegistry() / GetScriptManager().
	// These were public raw members that the old Scene::GetModuleScene() locator
	// chain reached through; the accessors are now the only supported path.
	Camera*        gameCamera    = nullptr;
	ScriptManager* scriptManager = nullptr;

	// Dependency Injection
	ModuleInput* mModuleInput;
	ModuleResourceManager* mModuleResourceManager;
	ModuleAudio* mModuleAudio;
	ModuleVideo* mModuleVideo;

	bool m_snapshotEnabled = false;

	// Owned here so its address is stable for the lifetime of the module; every
	// Scene created by this module holds a pointer to it. Populated by
	// Application via SetComponentServices, all-null until then.
	ComponentServices m_componentServices;

	SceneRenderData m_renderData;

	// ---------------------------------------------------------------------------
	// Simulation state
	// ---------------------------------------------------------------------------
	SimulationState m_simulationState  = SimulationState::STOPPED;
	bool            m_stepOneFrame     = false;  // consume in Update() to tick one frame while PAUSED
	bool            m_didStepThisFrame = false;  // propagates to PostUpdate() for LateUpdate gating
	std::atomic<bool> m_pendingStop          = false;  // deferred LoadScene flag — set by PressStop(), consumed in PreUpdate()
	std::atomic<bool> m_isLoadingScene       = false;  // true while a LoadSceneAsync job is in flight; guards re-entrancy
	std::string     m_snapshotPath     = "Library/_simulation_snapshot.nous";
	std::string     m_currentScenePath;  // empty = unsaved; set by LoadScene/SaveScene

	// After a scene is deserialized, re-instantiates any GO that carries a CPrefab
	// component from its source .nprefab file, replacing stale inline children.
	void RefreshPrefabInstances() const;

	// entt on_destroy<CEntityInfo> observer. Prunes the selection when any
	// GameObject dies, so Scene never needs to know that selection exists — the
	// module watches the scene instead of the scene telling the module
	// (Editor-Observes-Engine rule).
	void OnEntityDestroyed(entt::registry& registry, entt::entity entity);

	// Connect/disconnect the observer around a Scene's lifetime. MUST be paired:
	// an observer still connected when its Scene dies is fine (the signal dies with
	// the registry), but a Scene outliving THIS module would leave a dangling
	// `this` on the sink. Connect right after creating a Scene, disconnect right
	// before destroying it.
	void ConnectSceneObservers(Scene* scene);
	void DisconnectSceneObservers(Scene* scene);
};