#ifndef MODULESCENE_H
#define MODULESCENE_H

#include "Engine/Modules/Module.h"
#include "Engine/EngineExport.h"
#include <string>
#include <vector>
#include <mutex>
#include "Engine/Core/EventSystem/IEventListener.h"
#include "Engine/Utils/DataStructures/NOUS_Vector.h"

class Scene;
class GameObject;
class Camera;
class CScript;
class ScriptManager;

enum class SimulationState { STOPPED, PLAYING, PAUSED };

class ModuleScene : public Module, public IEventListener
{
public:

	// Constructor
	ModuleScene(Application* app);

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

	// Instantiates a .nprefab file into the active scene, optionally under parentGO.
	// Returns the root of the instantiated prefab, or nullptr on failure.
	NOUS_ENGINE_API GameObject* InstantiatePrefab(const std::string& path, GameObject* parentGO = nullptr);

	// Creates a root GameObject + one child GO per submesh for the given mesh
	// asset.  Each child has CTransform (from the node's local transform),
	// CMesh (individual submesh resource), and default CMaterial.
	// Safe to call from a job thread via CreateGameObjectDetached + RegisterGameObject.
	NOUS_ENGINE_API void SpawnMeshAsHierarchy(const std::string& assetsPath);

	// Called by CScript::OnStart / CScript::OnDestroy to maintain the live registry
	NOUS_ENGINE_API void RegisterScriptComponent(CScript* component);
	NOUS_ENGINE_API void UnregisterScriptComponent(CScript* component);

	// ---------------------------------------------------------------------------
	// Simulation controls
	// ---------------------------------------------------------------------------
	NOUS_ENGINE_API void PressPlay();   // STOPPED → PLAYING
	NOUS_ENGINE_API void PressStop();   // PLAYING/PAUSED → STOPPED (restores scene snapshot)
	NOUS_ENGINE_API void PressPause();  // PLAYING ↔ PAUSED toggle
	NOUS_ENGINE_API void PressStep();   // Advances exactly one frame while PAUSED

	NOUS_ENGINE_API bool            IsPlaying()           const { return m_simulationState == SimulationState::PLAYING; }
	NOUS_ENGINE_API bool            IsPaused()            const { return m_simulationState == SimulationState::PAUSED;  }
	NOUS_ENGINE_API bool            IsStopped()           const { return m_simulationState == SimulationState::STOPPED; }
	NOUS_ENGINE_API SimulationState GetSimulationState()  const { return m_simulationState; }

public:

	Scene*         activeScene    = nullptr;
	GameObject*    selectedGameObject = nullptr;
	Camera*        gameCamera    = nullptr;
	ScriptManager* scriptManager = nullptr;

private:

	// Flat registry of every live CScript component in the scene.
	// Used for hot-reload and LateUpdate dispatch.
	NOUS_Vector<CScript*> m_scriptComponents;
	std::mutex            m_scriptComponentsMutex;

	// ---------------------------------------------------------------------------
	// Simulation state
	// ---------------------------------------------------------------------------
	SimulationState m_simulationState  = SimulationState::STOPPED;
	bool            m_stepOneFrame     = false;  // consume in Update() to tick one frame while PAUSED
	bool            m_didStepThisFrame = false;  // propagates to PostUpdate() for LateUpdate gating
	bool            m_pendingStop             = false;  // deferred LoadScene flag — set by PressStop(), consumed in PreUpdate()
	bool            m_pendingPrefabRefresh    = false;  // set by LoadSceneAsync job, consumed in PreUpdate() on main thread
	std::string     m_snapshotPath     = "Library/_simulation_snapshot.nous";

	void RecompileScripts();
	void CleanupScripts();

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