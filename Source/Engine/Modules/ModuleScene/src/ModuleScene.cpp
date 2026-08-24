#include "Engine/Modules/ModuleScene/include/ModuleScene.h"
#include <vector>
#include "Engine/Core/FileSystem/FileSystem.h"
#include "Engine/Modules/ModuleInput/include/ModuleInput.h"
#include "Engine/Modules/ModuleResourceManager/include/ModuleResourceManager.h"
#include "Engine/Core/EventSystem/EventSystem.h"
#include "Engine/Core/Logger/Logger.h"

#include "Engine/Systems/ResourceManager/Types/ResourceMesh/include/ResourceMesh.h"
#include "Engine/Systems/ResourceManager/Types/ResourceMaterial/include/ResourceMaterial.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/Systems/CameraSystem/Camera/include/Camera.h"

#include "Engine/Scripting/ScriptManager.h"

#include "Engine/Systems/ECS/Scene/include/Scene.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"

#include "Engine/Systems/ECS/Component/Types/CMesh/include/CMesh.h"
#include "Engine/Systems/ECS/Component/Types/CMaterial/include/CMaterial.h"
#include "Engine/Systems/ECS/Component/Types/CTransform/include/CTransform.h"
#include "Engine/Systems/ECS/Component/Types/CCamera/include/CCamera.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include "Engine/NOUS_Multithreading/NOUS_JobSystem/include/NOUS_JobSystem.h"
#include "Engine/NOUS_Multithreading/NOUS_Thread/include/NOUS_Thread.h"

#include <future>

#ifdef _PROFILING
#include <tracy/Tracy.hpp>
#endif

#include "Engine/Core/TimeManager/TimeManager.h"

#include <SDL3/SDL.h>
#include <filesystem>
#include <algorithm>

#include "Engine/NOUS_Multithreading/NOUS_ThreadPool/include/NOUS_ThreadPool.h"
#include "Engine/Systems/ResourceManager/Types/ResourceMesh/include/ImporterMesh.h"
#include "Engine/Systems/ResourceManager/Core/ResourceBase/include/MetaFileData.h"
#include "Engine/Systems/PrefabManager/include/PrefabManager.h"
#include "Engine/Systems/ECS/Component/Types/CPrefab/include/CPrefab.h"

ModuleScene::ModuleScene(EventSystem* eventSystem, nous::engine::multithreading::NOUS_JobSystem* jobSystem,
    ModuleInput* moduleInput, ModuleResourceManager* moduleResourceManager, ModuleAudio* moduleAudio,
    ModuleVideo* moduleVideo)
    : Module(eventSystem, jobSystem), mModuleInput(moduleInput), mModuleResourceManager(moduleResourceManager),
      mModuleAudio(moduleAudio), mModuleVideo(moduleVideo)
{
	scriptManager = NOUS_NEW<ScriptManager>(MemoryTag::SCRIPTING_SYSTEM, mModuleInput, this);
	activeScene   = NOUS_NEW<Scene>(MemoryTag::SCENE, "Untitled Scene", this, mModuleResourceManager,
	                                &m_componentServices);
	gameCamera    = NOUS_NEW<Camera>(MemoryTag::CAMERA);

	// Load the script library — path is exe-relative so it works regardless of working directory.
	// SDL3's SDL_GetBasePath() returns a const char* managed internally by SDL — do NOT free it.
#if defined(_WIN32)
	constexpr auto kScriptsLib = "Scripts.dll";
#elif defined(__APPLE__)
	constexpr auto kScriptsLib = "Scripts.dylib";
#else
	constexpr auto kScriptsLib = "Scripts.so";
#endif
	const std::string scriptsDllPath =
        (std::filesystem::path(SDL_GetBasePath()) / "Library" / "Scripts" / kScriptsLib).string();
	if (!std::filesystem::exists(scriptsDllPath))
	{
		NOUS_WARN("Script library not found — attempting to build from source...");
		scriptManager->ReloadScriptLibrary(scriptsDllPath);
	}
	else if (!scriptManager->LoadScriptLibrary(scriptsDllPath))
	{
		NOUS_ERROR("Failed to load script library on startup");
	}

	eventSystem->Subscribe(EventType::WINDOW_RESIZED, this);
}

void ModuleScene::SetComponentServices(const ComponentServices& services)
{
	// Fills the object every live Scene already points at — see the header comment.
	m_componentServices = services;
}

ModuleScene::~ModuleScene()
{
	ClearSelection();
	NOUS_DELETE(gameCamera, MemoryTag::CAMERA);
	NOUS_DELETE(activeScene, MemoryTag::SCENE);
	NOUS_DELETE(scriptManager, MemoryTag::SCRIPTING_SYSTEM);
}

bool ModuleScene::Awake()
{
	gameCamera->SetPos(-4.61f, 100.0f, 718.32f);

	return true;
}

bool ModuleScene::Start()
{
	return true;
}

UpdateStatus ModuleScene::PreUpdate(float dt)
{
#ifdef _PROFILING
	ZoneScopedN("ModuleScene::PreUpdate");
#endif
	// Deferred stop: PressStop() set this flag instead of calling LoadScene() directly,
	// so that the scene is never cleared while the SCENE/GAME command buffers are still
	// recorded but not yet submitted (EndFrame hasn't run). By the time PreUpdate() is
	// reached, the previous frame's EndFrame has fully submitted — safe to reload.
	if (m_pendingStop)
	{
		m_pendingStop = false;
		LoadScene(m_snapshotPath);
		NOUS_INFO("[Scene] Simulation stopped — scene restored from snapshot.");
	}

	return UpdateStatus::CONTINUE;
}

UpdateStatus ModuleScene::Update(const float dt)
{
#ifdef _PROFILING
    ZoneScopedN("ModuleScene::Update");
#endif
	// Compute simulation dt — non-zero only when simulation is ticking.
	m_didStepThisFrame = false;
	float simDt = 0.0f;

	if (m_simulationState == SimulationState::PLAYING)
	{
		simDt = dt;
	}
	else if (m_simulationState == SimulationState::PAUSED && m_stepOneFrame)
	{
		simDt              = dt;
		m_stepOneFrame     = false;
		m_didStepThisFrame = true;
	}

	TimeManager::simulationDeltaTime = simDt;
	if (simDt > 0.0f)
	{
		TimeManager::simulationTime += simDt;
		TimeManager::simulationFrameCount++;
	}

	// Always update the scene — CTransform, CCamera, etc. need to run every frame for
	// rendering and editor-mode interactions. Scripts naturally don't tick in STOPPED mode
	// because m_instances is empty (CScript::OnUpdate iterates nothing).
	// Pass simDt so script Update() receives 0 when paused/stopped, full dt when playing.
	if (activeScene)
		activeScene->Update(simDt);

	return UpdateStatus::CONTINUE;
}

bool ModuleScene::IsSelected(const GameObject go) const
{
    return std::find(selectedGameObjects.begin(), selectedGameObjects.end(), go)
           != selectedGameObjects.end();
}

void ModuleScene::AddToSelection(const GameObject go)
{
    if (!go.IsValid() || IsSelected(go)) return;
    selectedGameObjects.push_back(go);
    primarySelection = go;
}

void ModuleScene::RemoveFromSelection(const GameObject go)
{
    auto it = std::find(selectedGameObjects.begin(), selectedGameObjects.end(), go);
    if (it == selectedGameObjects.end()) return;
    selectedGameObjects.erase(it);
    primarySelection = selectedGameObjects.empty() ? GameObject{} : selectedGameObjects.back();
}

void ModuleScene::SetSelection(const GameObject go)
{
    selectedGameObjects.clear();
    if (!go.IsValid()) { primarySelection = {}; return; }
    selectedGameObjects.push_back(go);
    primarySelection = go;
}

void ModuleScene::ClearSelection()
{
    selectedGameObjects.clear();
    primarySelection = {};
}

UpdateStatus ModuleScene::PostUpdate(float dt)
{
#ifdef _PROFILING
    ZoneScopedN("ModuleScene::PostUpdate");
#endif

	// Dispatch LateUpdate only when the simulation actually ticked this frame.
	if (m_simulationState == SimulationState::PLAYING || m_didStepThisFrame)
	{
#ifdef _PROFILING
		ZoneScopedN("DispatchLateUpdate");
#endif
		scriptManager->DispatchLateUpdate(TimeManager::simulationDeltaTime);
	}

	// Propagate parent transforms top-down before the renderer reads world matrices.
	// Skipped while LoadSceneAsync is in flight: the scene has already been cleared but
	// its replacement is still being preloaded on a worker, so there is nothing useful
	// to propagate yet. Deserialize now runs on the main thread (in the PreUpdate drain)
	// and ends with its own UpdateWorldMatrices pass, so by the time m_isLoadingScene
	// clears every world matrix is already correct.
	if (activeScene && !m_isLoadingScene.load(std::memory_order_acquire))
	{
#ifdef _PROFILING
        ZoneScopedN("UpdateWorldMatrices");
#endif
		activeScene->UpdateWorldMatrices();
	}

	// Build per-frame render snapshot consumed by ModuleRenderer3D::PostUpdate.
	m_renderData                = {};
	m_renderData.hasActiveScene = activeScene != nullptr;
	m_renderData.gameCamera     = gameCamera;
	m_renderData.selectedObjects = selectedGameObjects;
	m_renderData.primaryObject   = primarySelection;
	m_renderData.registry       = activeScene ? &activeScene->GetRegistry() : nullptr;

	return UpdateStatus::CONTINUE;
}

bool ModuleScene::CleanUp()
{
	// Wait for any in-flight jobs (e.g. hot-reload) before touching scripts
	JobSystem->WaitForPendingJobs();

	scriptManager->CleanupScripts();
	scriptManager->UnloadScriptLibrary();

	return true;
}

void ModuleScene::OnEvent(const Event& event)
{
	switch (event.type)
	{
		case EventType::WINDOW_RESIZED:
		{
			NOUS_DEBUG("WINDOW RESIZED EVENT");
			NOUS_DEBUG("Received context: %d, %d", event.ctx.i32[0], event.ctx.i32[1]);

			const float newAspect = static_cast<float>(event.ctx.i32[0]) / static_cast<float>(event.ctx.i32[1]);

			m_windowAspect = newAspect;

			// Update the legacy fallback camera.
			gameCamera->SetAspectRatio(newAspect);

			// Also update any CCamera components so the frustum visualization stays correct.
			if (activeScene)
			{
				for (auto [entity, cam] : activeScene->GetRegistry().view<CCamera>().each())
					cam.aspectRatio = newAspect;
			}

			break;
		}
		default: break;
	}
}

// ---------------------------------------------------------------------------
// Hot-reload
// ---------------------------------------------------------------------------

void ModuleScene::RecompileScripts() const
{
	scriptManager->RecompileScripts();
}

// ---------------------------------------------------------------------------
// Simulation controls
// ---------------------------------------------------------------------------

void ModuleScene::PressPlay()
{
	if (m_simulationState != SimulationState::STOPPED)
		return;

	// If a stop is still pending (very unlikely but possible if Play is pressed before
	// PreUpdate fires), cancel the pending reload — we're about to start fresh.
	m_pendingStop = false;

	// Wait for any in-flight jobs (e.g. the async Deserialize from a previous PressStop).
	// Without this, a rapid Stop → Play sequence would serialize a partially-constructed
	// scene (CMesh/CMaterial resources not yet assigned), corrupting the snapshot and
	// causing null ResourceBase* dereferences the next time the snapshot is loaded.
	JobSystem->WaitForPendingJobs();

	// Save a snapshot so PressStop can restore the scene to its pre-play state.
	if (m_snapshotEnabled)
	{
		std::filesystem::create_directories(std::filesystem::path(m_snapshotPath).parent_path());
		activeScene->Serialize(m_snapshotPath);
	}

	// Instances already exist (created in edit mode at CScript::OnStart so the
	// Inspector could edit their fields). Just fire Awake/Start on them.
	scriptManager->StartAllInstances();

	m_simulationState = SimulationState::PLAYING;

	TimeManager::simulationTime       = 0.0f;
	TimeManager::simulationDeltaTime  = 0.0f;
	TimeManager::simulationFrameCount = 0;
	TimeManager::gameTimer.Start();

	NOUS_INFO("[Scene] Simulation started.");
}

void ModuleScene::PressStop()
{
	if (m_simulationState == SimulationState::STOPPED)
		return;

	m_simulationState             = SimulationState::STOPPED;
	m_stepOneFrame                = false;
	m_didStepThisFrame            = false;
	TimeManager::simulationDeltaTime  = 0.0f;
	TimeManager::simulationTime       = 0.0f;
	TimeManager::simulationFrameCount = 0;

	// Defer the actual scene reload to PreUpdate() so that LoadScene() / ClearScene()
	// never run while the SCENE or GAME command buffers are recorded but not yet
	// submitted by EndFrame(). DestroyGeometry() calls vkDeviceWaitIdle, which only
	// waits for already-submitted work — not for recorded-but-pending command buffers.
	// PreUpdate() is guaranteed to run after the previous frame's EndFrame has submitted.
	m_pendingStop = true;
}

void ModuleScene::PressPause()
{
	if (m_simulationState == SimulationState::PLAYING)
	{
		m_simulationState = SimulationState::PAUSED;
		NOUS_INFO("[Scene] Simulation paused.");
	}
	else if (m_simulationState == SimulationState::PAUSED)
	{
		m_simulationState = SimulationState::PLAYING;
		NOUS_INFO("[Scene] Simulation resumed.");
	}
}

void ModuleScene::PressStep()
{
	if (m_simulationState != SimulationState::PAUSED)
		return;

	m_stepOneFrame = true;
	NOUS_INFO("[Scene] Simulation stepping one frame.");
}

void ModuleScene::SaveScene(const std::string& path)
{
    // Name the in-memory scene after the file stem so the Inspector/Hierarchy
    // reflect the saved name without requiring a reload.
    const std::string filename = nous::engine::filesystem::GetFilename(path);
    if (const std::string stem = std::filesystem::path(filename).stem().string(); !stem.empty())
        activeScene->SetName(stem);

    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    activeScene->Serialize(path);

    // Route the freshly-written scene through the import pipeline: it creates the
    // .meta (with a stable UID) if absent and mirrors the source into its UID-keyed
    // Library/Scenes/<uid>.nous, exactly like every other asset. Then refresh the
    // scene manifest so GAME mode resolves this scene immediately, without waiting
    // for a full asset rescan.
    mModuleResourceManager->ImportFile(path);
    mModuleResourceManager->RefreshSceneManifest();

    m_currentScenePath = path;
}

namespace
{
	// Resolves a scene reference to a concrete, loadable file path.
	// - If the reference already points to an existing file, use it directly:
	//   covers EDITOR Assets/ paths, the simulation snapshot, and already
	//   UID-keyed library paths.
	// - Otherwise treat the filename stem as a scene name and resolve it through
	//   scene_manifest.json to Library/Scenes/<uid>.nous. This is the GAME-mode
	//   path: game_config startScene is "Library/Scenes/<name>.nous", which no
	//   longer exists as a file now that scenes are stored UID-keyed.
	// Returns "" when the reference can be resolved neither way.
	std::string ResolveScenePath(const std::string& reference)
	{
		if (nous::engine::filesystem::Exists(reference))
			return reference;

		const std::string sceneName = nous::engine::filesystem::GetFilename(reference); // stem, no ext
		const std::string resolved  = ImportPipeline::ResolveSceneLibraryPath(sceneName);
		if (resolved.empty())
			NOUS_ERROR("[ModuleScene] Cannot resolve scene '%s': not a file on disk and no manifest entry for '%s'.",
				reference.c_str(), sceneName.c_str());
		return resolved;
	}
}

void ModuleScene::LoadScene(const std::string& path)
{
	// Drain any in-flight jobs (e.g. debug hotkey loaders) before clearing the
	// scene. Without this, a job that called CreateGameObjectDetached before the
	// clear could still call RegisterGameObject afterward on a cleared scene.
	JobSystem->WaitForPendingJobs();

	// Resolve before clearing so an unresolvable reference doesn't nuke the
	// current scene. Keep the original `path` for snapshot comparison and the
	// user-facing m_currentScenePath; only the load uses the resolved path.
	const std::string loadPath = ResolveScenePath(path);
	if (loadPath.empty())
		return;

	ClearScene();

	// Pre-load all mesh resources in parallel before building the scene graph.
	// CMesh::Deserialize() will hit the resource cache (no disk I/O) instead of
	// blocking serially on each binary file read.
	for (auto futures = mModuleResourceManager->PreloadSceneResourcesAsync(JobSystem, loadPath); auto& f : futures)
		f.get();

	activeScene->Deserialize(loadPath);
	// Snapshot already captures the complete pre-play state of every entity,
	// including prefab instances. Refreshing from disk would destroy and recreate
	// prefab children from their .nprefab source files, resetting them to their
	// initial (disk) state rather than the editor state at the time Play was pressed.
	if (path != m_snapshotPath)
		RefreshPrefabInstances();

	// Don't treat the simulation snapshot as the user's active scene — otherwise
	// pressing Stop would make Save overwrite Library/_simulation_snapshot.nous.
	if (path != m_snapshotPath)
		m_currentScenePath = path;
}

void ModuleScene::LoadSceneAsync(const std::string& path)
{
	// Re-entrancy guard: if a load is already in flight, ignore the new request.
	// Without this, spamming the hotkey clears the scene while the in-flight job
	// still holds pointers into it → use-after-free.
	if (bool expected = false; !m_isLoadingScene.compare_exchange_strong(expected, true))
		return;

	// Drain any in-flight jobs (e.g. debug hotkey loaders) before clearing the
	// scene. Without this, a job that called CreateGameObjectDetached before the
	// clear could still call RegisterGameObject afterward on a cleared scene.
	JobSystem->WaitForPendingJobs();

	// Resolve on the main thread (cheap file I/O) so the worker only deserializes.
	// Release the in-flight guard we just took if the reference can't be resolved.
	const std::string loadPath = ResolveScenePath(path);
	if (loadPath.empty())
	{
		m_isLoadingScene = false;
		return;
	}

	ClearScene();

	// Track the scene path up-front — the worker thread only does deserialization,
	// and skipping snapshot reloads keeps PressStop from hijacking the active path.
	if (path != m_snapshotPath)
		m_currentScenePath = path;

	JobSystem->SubmitJob([this, loadPath]
		{
			// Pre-load all mesh resources in parallel only when there are at least 2 worker
			// threads. This job occupies 1 thread while it blocks on future.get() — with only
			// 1 worker thread available the sub-jobs could never start, causing a deadlock.
			// With 0 threads the job system runs everything synchronously on the main thread,
			// so futures complete immediately and are also safe (but parallel gains nothing).
			if (JobSystem->GetThreadPool().GetThreads().size() >= 2)
			{
				for (auto futures = mModuleResourceManager->PreloadSceneResourcesAsync(JobSystem, loadPath); auto& f : futures)
					f.get();
			}

			// Scene graph construction mutates the registry, so it runs on the main
			// thread. Resource lookups hit the cache filled by the preload above.
			// RefreshPrefabInstances can be called inline here: the reason it used to
			// be deferred (DestroyGameObject → CMesh::OnDestroy → vkDeviceWaitIdle
			// deadlocking from a worker) no longer applies on the main thread.
			JobSystem->SubmitToMainThread([this, loadPath]
				{
					activeScene->Deserialize(loadPath);
					RefreshPrefabInstances();
					m_isLoadingScene = false;
				}, "Apply scene");
		}
	);
}

GameObject ModuleScene::InstantiatePrefab(const std::string& path, GameObject parent) const
{
	return PrefabManager::InstantiatePrefab(path, activeScene, parent);
}

void ModuleScene::ClearScene()
{
    ClearSelection();
    activeScene->Clear();
}

void ModuleScene::NewScene(const std::string& name)
{
    // Drain in-flight scene jobs before clearing — same reasoning as LoadScene.
    JobSystem->WaitForPendingJobs();

    ClearScene();
    activeScene->SetName(name);
    m_currentScenePath.clear();

    NOUS_INFO("[Scene] New scene '%s' created.", name.c_str());
}

void ModuleScene::SpawnMeshAsHierarchy(const std::string& assetsPath) const
{
    // 1. Read meta to get library path and verify the asset exists.
    MetaFileData metaData;
    if (!ImportPipeline::GetAssetMetaData(assetsPath, metaData))
    {
        NOUS_ERROR("[SpawnMeshAsHierarchy] No meta file for '%s'. Import it first.", assetsPath.c_str());
        return;
    }

    // 2. Load the full submesh hierarchy from the library binary.
    const auto submeshes = ImporterMesh::LoadHierarchy(metaData.libraryPath);
    if (submeshes.empty())
    {
        NOUS_ERROR("[SpawnMeshAsHierarchy] No submeshes in '%s'.", metaData.libraryPath.c_str());
        return;
    }

    // 3. Load all submesh resources in parallel.
    //    SpawnMeshAsHierarchy is always called from a job thread, so futures are used
    //    instead of WaitForPendingJobs() (which would deadlock — see LoadSceneAsync).
    //    Guard: requires >= 2 worker threads; with only 1 this job would block on
    //    future.get() while sub-jobs sit in the queue with no thread to run them.
    const auto submeshCount = static_cast<int32_t>(submeshes.size());
    std::vector<ResourceMesh*> meshResources(submeshCount, nullptr);

    if (JobSystem->GetThreadPool().GetThreads().size() >= 2)
    {
        std::vector<std::promise<void>> promises(submeshCount);
        std::vector<std::future<void>>  futures;
        futures.reserve(submeshCount);

        for (int32_t i = 0; i < submeshCount; ++i)
        {
            futures.push_back(promises[i].get_future());
            auto* promPtr = &promises[i];

            JobSystem->SubmitJob([this, &assetsPath, &meshResources, i, promPtr]
            {
                meshResources[i] = mModuleResourceManager->RequestOrCreateSubMeshResource(assetsPath, i);
                promPtr->set_value();
            }, "Load Submesh " + std::to_string(i));
        }

        for (auto& f : futures) f.get();
    }
    else
    {
        for (int32_t i = 0; i < submeshCount; ++i)
            meshResources[i] = mModuleResourceManager->RequestOrCreateSubMeshResource(assetsPath, i);
    }

    // 4. Build the plain-data description. Everything expensive — resource
    //    resolution and the matrix decomposition — happens here, on the worker.
    PendingModelSpawn spawn;
    spawn.rootName = std::filesystem::path(assetsPath).filename().string();
    spawn.submeshes.reserve(static_cast<size_t>(submeshCount));

    for (int32_t i = 0; i < submeshCount; ++i)
    {
        const SubMeshData& sub = submeshes[static_cast<size_t>(i)];

        if (!meshResources[i])
        {
            NOUS_WARN("[SpawnMeshAsHierarchy] Failed to create sub-resource for submesh %d of '%s'.",
                i, assetsPath.c_str());
            continue;
        }

        PendingSubMesh pending;
        pending.name         = sub.name;
        pending.mesh         = meshResources[i];
        pending.submeshIndex = i;

        // Apply the node's accumulated world transform as the child's local transform.
        glm::vec3 skew;
        glm::vec4 persp;
        glm::decompose(sub.localTransform, pending.scale, pending.orientation, pending.position, skew, persp);

        // If the import baked a per-submesh material (V3 binary), resolve it via
        // the ResourceManager. Falls back to the default material when the field
        // is empty (V2 binary) or the .nmat asset is missing/unloadable.
        // Safe off-thread: ResourceTable has its own mutex and the refcount is atomic.
        ResourceMaterial* resolved = nullptr;
        if (!sub.materialAssetPath.empty())
        {
            resolved = down_cast<ResourceMaterial*>(
                mModuleResourceManager->CreateResource(sub.materialAssetPath));
            if (!resolved)
            {
                NOUS_WARN("[SpawnMeshAsHierarchy] Material '%s' (submesh %d of '%s') "
                          "failed to load — using default.",
                          sub.materialAssetPath.c_str(), i, assetsPath.c_str());
            }
        }
        // Resolved here rather than in the build step so BuildModelHierarchyInto
        // stays free of the ResourceManager and remains unit-testable.
        pending.material = resolved ? resolved : mModuleResourceManager->GetDefaultMaterial();

        spawn.submeshes.push_back(std::move(pending));
    }

    // 5. Hand off to the main thread. `spawn` is MOVED into the lambda: capturing
    //    it by reference would dangle, since this function returns long before the
    //    task runs. `mutable` is what allows moving out of the capture.
    const std::string taskName = "Build " + spawn.rootName;
    JobSystem->SubmitToMainThread(
        [this, spawn = std::move(spawn)]() mutable
        {
            BuildModelHierarchyInto(*activeScene, std::move(spawn));
        },
        taskName);
}

void BuildModelHierarchyInto(Scene& scene, PendingModelSpawn&& spawn)
{
    NOUS_ASSERT_MAIN_THREAD();

    // Root GO — named after the file, no mesh of its own.
    GameObject rootGO = scene.CreateGameObjectDetached(spawn.rootName);

    // One child GO per submesh.
    for (PendingSubMesh& sub : spawn.submeshes)
    {
        GameObject childGO = scene.CreateGameObjectDetached(sub.name, &rootGO);

        if (auto* t = childGO.TryGetComponent<CTransform>())
        {
            t->position    = sub.position;
            t->orientation = sub.orientation;
            t->scale       = sub.scale;
            t->eulerHint   = t->GetEulerAngles();
            t->UpdateMatrix();
        }

        auto& meshComp        = childGO.AddComponent<CMesh>();
        meshComp.mesh         = sub.mesh;
        meshComp.submeshIndex = sub.submeshIndex;

        auto& matComp = childGO.AddComponent<CMaterial>();
        matComp.material = sub.material;

        scene.RegisterGameObject(childGO);
    }

    // Register root last so children are already in the scene list.
    scene.RegisterGameObject(rootGO);

    NOUS_INFO("[SpawnMeshAsHierarchy] Spawned '%s' with %zu submesh(es).",
        spawn.rootName.c_str(), spawn.submeshes.size());
}

void ModuleScene::BuildModelHierarchy(PendingModelSpawn&& spawn)
{
    BuildModelHierarchyInto(*activeScene, std::move(spawn));
}

bool ModuleScene::HasMainCamera() const
{
    if (!activeScene)
        return false;

    for (auto [entity, cam] : activeScene->GetRegistry().view<CCamera>().each())
    {
        if (cam.isMainCamera)
            return true;
    }
    return false;
}


void ModuleScene::RefreshPrefabInstances() const
{
    if (!activeScene) return;

    // Phase 1: collect prefab roots from a view snapshot.
    // We must NOT call ReloadPrefabInstance while iterating — reload destroys
    // children that would also appear in the view, leaving dangling handles.
    std::vector<GameObject> prefabRoots;
    {
        auto& registry = activeScene->GetRegistry();
        for (auto entity : registry.view<CPrefab>())
            prefabRoots.emplace_back(entity, &registry);
    }

    if (prefabRoots.empty())
    {
        NOUS_INFO("[Scene] RefreshPrefabInstances: no prefab instances found in scene.");
        return;
    }

    NOUS_INFO("[Scene] RefreshPrefabInstances: refreshing %zu prefab instance(s).", prefabRoots.size());

    // Phase 2: reload each root now that the snapshot is discarded.
    for (auto root : prefabRoots)
        PrefabManager::ReloadPrefabInstance(root, activeScene);

    NOUS_INFO("[Scene] RefreshPrefabInstances: done.");
}
