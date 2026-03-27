#include "Engine/Modules/ModuleScene/include/ModuleScene.h"
#include "Engine/Core/FileSystem/FileSystem.h"
#include "Engine/Modules/ModuleInput/include/ModuleInput.h"
#include "Engine/Modules/ModuleResourceManager/include/ModuleResourceManager.h"
#include "Engine/Core/EventSystem/EventSystem.h"
#include "Engine/Core/Logger/Logger.h"

#include "Engine/Systems/ResourceManager/Resource/ResourceMesh/include/ResourceMesh.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/Systems/CameraSystem/Camera/include/Camera.h"

#include "Engine/Scripting/ScriptManager.h"

#include "Engine/Systems/ECS/Scene/include/Scene.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"

#include "Engine/Systems/ECS/Component/CMesh/include/CMesh.h"
#include "Engine/Systems/ECS/Component/CMaterial/include/CMaterial.h"
#include "Engine/Systems/ECS/Component/CTransform/include/CTransform.h"
#include "Engine/Systems/ECS/Component/CCamera/include/CCamera.h"
#include "Engine/Systems/ECS/Component/CScript/include/CScript.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include "Engine/NOUS_Multithreading/NOUS_JobSystem/include/NOUS_JobSystem.h"
#include "Engine/NOUS_Multithreading/NOUS_Thread/include/NOUS_Thread.h"

#include "Engine/Core/TimeManager/TimeManager.h"

#include <SDL3/SDL.h>
#include <filesystem>
#include <algorithm>

#include "Engine/Systems/ResourceManager/Resource/ResourceShader/include/ResourceShader.h"
#include "Engine/Systems/ResourceManager/Importer/ImporterMesh/include/ImporterMesh.h"
#include "Engine/Systems/ResourceManager/Resource/MetaFileData.inl"
#include "Engine/Systems/PrefabManager/include/PrefabManager.h"
#include "Engine/Systems/ECS/Component/CPrefab/include/CPrefab.h"

ModuleScene::ModuleScene(EventSystem* eventSystem, NOUS_Multithreading::NOUS_JobSystem* jobSystem, bool isGameMode,
    ModuleInput* moduleInput, ModuleResourceManager* moduleResourceManager)
    : Module(eventSystem, jobSystem), m_isGameMode(isGameMode), mModuleInput(moduleInput), mModuleResourceManager(moduleResourceManager),
	m_scriptComponents(MemoryTag::SCRIPTING_SYSTEM)
{
	activeScene   = NOUS_NEW<Scene>(MemoryTag::SCENE, "Untitled Scene", this, mModuleResourceManager);
	gameCamera    = NOUS_NEW<Camera>(MemoryTag::CAMERA);
	scriptManager = NOUS_NEW<ScriptManager>(MemoryTag::SCRIPTING_SYSTEM, mModuleInput, this);

	// Load the script library — path is exe-relative so it works regardless of working directory.
	// SDL3's SDL_GetBasePath() returns a const char* managed internally by SDL — do NOT free it.
#if defined(_WIN32)
	constexpr const char* kScriptsLib = "Scripts.dll";
#elif defined(__APPLE__)
	constexpr const char* kScriptsLib = "Scripts.dylib";
#else
	constexpr const char* kScriptsLib = "Scripts.so";
#endif
	const std::string scriptsDllPath =
        (std::filesystem::path(SDL_GetBasePath()) / "Library" / "Scripts" / kScriptsLib).string();
	if (!scriptManager->LoadScriptLibrary(scriptsDllPath))
		NOUS_ERROR("Failed to load script library on startup");

	eventSystem->Subscribe(EventType::WINDOW_RESIZED, this);
}

ModuleScene::~ModuleScene()
{
    selectedGameObject = nullptr;

	NOUS_DELETE(gameCamera, MemoryTag::CAMERA);
	NOUS_DELETE(scriptManager, MemoryTag::SCRIPTING_SYSTEM);
	NOUS_DELETE(activeScene, MemoryTag::SCENE);
}

bool ModuleScene::Awake()
{
	gameCamera->SetPos(-4.61f, 100.0f, 718.32f);

	return true;
}

bool ModuleScene::Start()
{
    // In GAME mode the GameApp controls which scene to load via an explicit
    // LoadSceneAsync() call after Start() returns.  Skip the auto-load here
    // to avoid a double-load race condition.
    if (!m_isGameMode)
        LoadSceneAsync("Assets/Scenes/LagiacrusScene.nous");

	return true;
}

UpdateStatus ModuleScene::PreUpdate(float dt)
{
	// Deferred stop: PressStop() set this flag instead of calling LoadScene() directly,
	// so that the scene is never cleared while the SCENE/GAME command buffers are still
	// recorded but not yet submitted (EndFrame hasn't run). By the time PreUpdate() is
	// reached, the previous frame's EndFrame has fully submitted — safe to reload.
	if (m_pendingStop)
	{
		m_pendingStop          = false;
		m_pendingPrefabRefresh = false; // LoadScene calls RefreshPrefabInstances directly
		LoadScene(m_snapshotPath);
		NOUS_INFO("[Scene] Simulation stopped — scene restored from snapshot.");
	}

	if (m_pendingPrefabRefresh)
	{
		m_pendingPrefabRefresh = false;
		RefreshPrefabInstances();
	}

	return UpdateStatus::CONTINUE;
}

UpdateStatus ModuleScene::Update(float dt)
{
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

	if (mModuleInput->GetKey(SDL_SCANCODE_M) == KeyState::DOWN)
	{
		ScriptManager::GenerateScript("PRUEBA_CREAR_SCRIPT_DESDE_MOTOR");
	}

    if (mModuleInput->GetKey(SDL_SCANCODE_Z) == KeyState::DOWN)
    {
        SaveScene("Assets/Scenes/LagiacrusScene.nous");
    }

	if (mModuleInput->GetKey(SDL_SCANCODE_X) == KeyState::DOWN)
	{
		ClearScene();
	}

	if (mModuleInput->GetKey(SDL_SCANCODE_C) == KeyState::DOWN)
	{
		LoadScene("Assets/Scenes/LagiacrusScene.nous");
	}

    if (mModuleInput->GetKey(SDL_SCANCODE_F1) == KeyState::DOWN)
    {
        JobSystem->SubmitJob([this]()
        {
            SpawnMeshAsHierarchy("Assets/Meshes/Lagiacrus_Head.fbx");
        }, "Spawn Lagiacrus");
    }

    if (mModuleInput->GetKey(SDL_SCANCODE_F2) == KeyState::DOWN)
    {
        JobSystem->SubmitJob([this]()
        {
            SpawnMeshAsHierarchy("Assets/Meshes/Cypher_S0_Skelmesh.fbx");
        }, "Spawn Cypher");
    }

    if (mModuleInput->GetKey(SDL_SCANCODE_F3) == KeyState::DOWN)
    {
        JobSystem->SubmitJob([this]()
        {
            SpawnMeshAsHierarchy("Assets/Meshes/Queen_Xenomorph.fbx");
        }, "Spawn Queen Xenomorph");
    }

    if (mModuleInput->GetKey(SDL_SCANCODE_F4) == KeyState::DOWN)
    {
        JobSystem->SubmitJob([this]()
        {
            SpawnMeshAsHierarchy("Assets/Meshes/Wolf.obj");
        }, "Spawn Wolf");
    }

    if (mModuleInput->GetKey(SDL_SCANCODE_F5) == KeyState::DOWN)
    {
        static const std::vector<std::string> meshPaths = {
            "Assets/Meshes/Lagiacrus_Head.fbx",
            "Assets/Meshes/Cypher_S0_Skelmesh.fbx",
            "Assets/Meshes/Queen_Xenomorph.fbx",
            "Assets/Meshes/Wolf.obj"
        };
        for (const auto& path : meshPaths)
        {
            JobSystem->SubmitJob([this, path]()
            {
                SpawnMeshAsHierarchy(path);
            }, "Spawn Model");
        }
    }

	if (mModuleInput->GetKey(SDL_SCANCODE_F6) == KeyState::DOWN)
	{
		ClearScene();
	}

	if (mModuleInput->GetKey(SDL_SCANCODE_F7) == KeyState::DOWN)
	{
		JobSystem->SubmitJob([this]()
								  {
									  NOUS_Multithreading::NOUS_Thread::SleepMS(5000);
								  }, "Test");
	}

	if (mModuleInput->GetKey(SDL_SCANCODE_F8) == KeyState::DOWN)
	{
		for (int i = 0; i < 100; ++i)
		{
			JobSystem->SubmitJob([]
									  {
										  std::chrono::milliseconds duration(500);
										  auto start = std::chrono::steady_clock::now();

										  while (std::chrono::steady_clock::now() - start < duration)
										  {
											  (void)std::sqrt(123.456); // Dummy CPU-bound work
										  }

									  }, "Stress Test");
		}
	}

	if (mModuleInput->GetKey(SDL_SCANCODE_F9) == KeyState::DOWN)
	{
		NOUS_INFO("Initiating script hot-reload...");
		JobSystem->SubmitJob([this] { RecompileScripts(); }, "Scripts Hot-Reload");
	}

	return UpdateStatus::CONTINUE;
}

UpdateStatus ModuleScene::PostUpdate(float dt)
{
	// Dispatch LateUpdate only when the simulation actually ticked this frame.
	if (m_simulationState == SimulationState::PLAYING || m_didStepThisFrame)
	{
		std::lock_guard<std::mutex> lock(m_scriptComponentsMutex);
		for (auto* cs : m_scriptComponents)
			if (cs) cs->LateUpdate(TimeManager::simulationDeltaTime);
	}

	// Propagate parent transforms top-down before the renderer reads world matrices.
	if (activeScene)
		activeScene->UpdateWorldMatrices();

	// Build per-frame render snapshot consumed by ModuleRenderer3D::PostUpdate.
	m_renderData                = {};
	m_renderData.hasActiveScene = (activeScene != nullptr);
	m_renderData.gameCamera     = gameCamera;
	m_renderData.selectedObject = selectedGameObject;
	if (activeScene)
		m_renderData.gameObjects = activeScene->GetGameObjectsSnapshot();

	return UpdateStatus::CONTINUE;
}

bool ModuleScene::CleanUp()
{
	// Wait for any in-flight jobs (e.g. hot-reload) before touching scripts
	JobSystem->WaitForPendingJobs();

	CleanupScripts();
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

			const float newAspect = (float)event.ctx.i32[0] / (float)event.ctx.i32[1];

			// Update the legacy fallback camera.
			gameCamera->SetAspectRatio(newAspect);

			// Also update any CCamera components so the frustum visualization stays correct.
			if (activeScene)
			{
				const auto gos = activeScene->GetGameObjectsSnapshot();
				for (const auto& go : gos)
				{
					if (auto* cam = go->TryGetComponent<CCamera>())
						cam->aspectRatio = newAspect;
				}
			}

			break;
		}
		default: break;
	}
}

// ---------------------------------------------------------------------------
// CScript component registry — called by CScript::OnStart / OnDestroy
// ---------------------------------------------------------------------------

void ModuleScene::RegisterScriptComponent(CScript* component)
{
	std::lock_guard<std::mutex> lock(m_scriptComponentsMutex);
	m_scriptComponents.push_back(component);
}

void ModuleScene::UnregisterScriptComponent(CScript* component)
{
	std::lock_guard<std::mutex> lock(m_scriptComponentsMutex);
	auto it = std::find(m_scriptComponents.begin(), m_scriptComponents.end(), component);
	if (it != m_scriptComponents.end())
		m_scriptComponents.erase(it);
}

// ---------------------------------------------------------------------------
// Hot-reload
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Hot-reload
// ---------------------------------------------------------------------------

void ModuleScene::RecompileScripts()
{
	const std::string dllPath =
		(std::filesystem::path(SDL_GetBasePath()) / "Library" / "Scripts" / "Scripts.dll").string();

	{
		std::lock_guard<std::mutex> lock(m_scriptComponentsMutex);
		for (auto* cs : m_scriptComponents)
			if (cs) cs->ClearInstances();
	}

	if (scriptManager->ReloadScriptLibrary(dllPath))
	{
		std::lock_guard<std::mutex> lock(m_scriptComponentsMutex);
		for (auto* cs : m_scriptComponents)
			if (cs) cs->RecreateInstances();
		NOUS_INFO("Script hot-reload completed successfully");
	}
	else
	{
		NOUS_ERROR("Script hot-reload failed");
	}
}

// ---------------------------------------------------------------------------
// Cleanup (called from CleanUp() before UnloadScriptLibrary)
// ---------------------------------------------------------------------------

void ModuleScene::CleanupScripts()
{
	std::lock_guard<std::mutex> lock(m_scriptComponentsMutex);

	// Destroy DLL-allocated instances and mark each component as unregistered.
	// ClearRegistrationState() ensures that the subsequent OnDestroy() calls
	// (from ~ModuleScene() → NOUS_DELETE(activeScene)) skip the UnregisterScriptComponent
	// call entirely — the scene module may be in a partially-destroyed state at that point.
	for (auto* cs : m_scriptComponents)
	{
		if (cs)
		{
			cs->ClearInstances();
			cs->ClearRegistrationState();
		}
	}

	m_scriptComponents.clear();
	NOUS_INFO("Cleaned up all CScript instances");
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
	// causing null Resource* dereferences the next time the snapshot is loaded.
	JobSystem->WaitForPendingJobs();

	// Save a snapshot of the current scene so PressStop can restore it.
	// Skipped in GAME mode — there is no editor Stop button, so the snapshot
	// is never needed and writing it would pollute the game's working directory.
	if (!m_isGameMode)
	{
		std::filesystem::create_directories(std::filesystem::path(m_snapshotPath).parent_path());
		activeScene->Serialize(m_snapshotPath);
	}

	// Start all registered CScript components (they were registered in OnStart but
	// deferred instance creation because the simulation was stopped).
	{
		std::lock_guard<std::mutex> lock(m_scriptComponentsMutex);
		for (auto* cs : m_scriptComponents)
			if (cs) cs->RecreateInstances();
	}

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
    activeScene->Serialize(path);

    // Mirror the saved scene to Library/Scenes/ so GameApp can load it
    // from Library without needing Assets/.
    const std::string filename = NOUS_FileManager::GetFilename(path);
    const std::string libraryPath = "Library/Scenes/" + filename;
    NOUS_FileManager::CopyFile(path, libraryPath);
}

void ModuleScene::LoadScene(const std::string& path)
{
	// Drain any in-flight jobs (e.g. debug hotkey loaders) before clearing the
	// scene. Without this, a job that called CreateGameObjectDetached before the
	// clear could still call RegisterGameObject afterward on a cleared scene.
	JobSystem->WaitForPendingJobs();

	ClearScene();
	activeScene->Deserialize(path);
	EnsureMainCamera();
	RefreshPrefabInstances();
}

void ModuleScene::LoadSceneAsync(const std::string& path)
{
	// Drain any in-flight jobs (e.g. debug hotkey loaders) before clearing the
	// scene. Without this, a job that called CreateGameObjectDetached before the
	// clear could still call RegisterGameObject afterward on a cleared scene.
	JobSystem->WaitForPendingJobs();

	ClearScene();

	JobSystem->SubmitJob([this, path]
		{
			activeScene->Deserialize(path);
			EnsureMainCamera();
			// RefreshPrefabInstances() must NOT run on the job thread — it calls
			// DestroyGameObject → CMesh::OnDestroy → vkDeviceWaitIdle, which deadlocks
			// when the main thread is rendering. Defer to PreUpdate() instead.
			m_pendingPrefabRefresh = true;
		}
	);
}

GameObject* ModuleScene::InstantiatePrefab(const std::string& path, GameObject* parentGO)
{
	return PrefabManager::InstantiatePrefab(path, activeScene, parentGO);
}

void ModuleScene::ClearScene()
{
    selectedGameObject = nullptr;
    activeScene->Clear();
}

void ModuleScene::SpawnMeshAsHierarchy(const std::string& assetsPath)
{
    // 1. Read meta to get library path and verify the asset exists.
    MetaFileData metaData;
    if (!mModuleResourceManager->GetAssetMetaData(assetsPath, metaData))
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

    // 3. Root GO — named after the file, no mesh of its own.
    const std::string modelName = std::filesystem::path(assetsPath).filename().string();
    GameObject* rootGO = activeScene->CreateGameObjectDetached(modelName);

    // 4. One child GO per submesh.
    for (int32_t i = 0; i < static_cast<int32_t>(submeshes.size()); ++i)
    {
        const SubMeshData& sub = submeshes[static_cast<size_t>(i)];

        ResourceMesh* meshResource =
            mModuleResourceManager->RequestOrCreateSubMeshResource(assetsPath, i);
        if (!meshResource)
        {
            NOUS_WARN("[SpawnMeshAsHierarchy] Failed to create sub-resource for submesh %d of '%s'.",
                i, assetsPath.c_str());
            continue;
        }

        // Create child GO attached to root.
        GameObject* childGO = activeScene->CreateGameObjectDetached(sub.name, rootGO);

        // Apply the node's accumulated world transform as the child's local transform.
        if (auto* t = childGO->TryGetComponent<CTransform>())
        {
            glm::vec3 pos, scale, skew;
            glm::vec4 persp;
            glm::quat orient;
            glm::decompose(sub.localTransform, scale, orient, pos, skew, persp);

            t->position    = pos;
            t->orientation = orient;
            t->scale       = scale;
            t->eulerHint   = t->GetEulerAngles();
            t->UpdateMatrix();
        }

        // Mesh component — references the individual submesh resource.
        auto& meshComp      = childGO->AddComponent<CMesh>();
        meshComp.mesh        = meshResource;
        meshComp.submeshIndex = i;

        // Default material — user can reassign via Inspector.
        auto& matComp    = childGO->AddComponent<CMaterial>();
        matComp.material = mModuleResourceManager->GetDefaultMaterial();

        activeScene->RegisterGameObject(childGO);
    }

    // 5. Register root last so children are already in the scene list.
    activeScene->RegisterGameObject(rootGO);

    NOUS_INFO("[SpawnMeshAsHierarchy] Spawned '%s' with %zu submesh(es).",
        modelName.c_str(), submeshes.size());
}

void ModuleScene::EnsureMainCamera()
{
    if (!activeScene)
        return;

    // Check whether the loaded scene already has a main camera.
    const auto gameObjects = activeScene->GetGameObjectsSnapshot();
    for (const auto& go : gameObjects)
    {
        if (auto* cam = go->TryGetComponent<CCamera>())
        {
            if (cam->isMainCamera)
                return; // Found one — nothing to do.
        }
    }

    // No main camera found. Create a default one that mirrors the legacy gameCamera.
    NOUS_INFO("No main CCamera found in scene — creating default 'Main Camera' GameObject.");

    GameObject* cameraGO = activeScene->CreateGameObject("Main Camera", nullptr);

    // Position from the legacy orphan Camera so the view doesn't jump.
    if (auto* t = cameraGO->TryGetComponent<CTransform>())
    {
        t->SetPosition(gameCamera->GetPos());
        // Derive orientation from the legacy camera's front/up vectors.
        const glm::vec3 fwd = gameCamera->GetFront();
        const glm::vec3 up  = gameCamera->GetUp();
        const glm::vec3 right = glm::normalize(glm::cross(fwd, up));
        const glm::mat3 rotMat(right, up, -fwd); // column-major: right, up, -forward
        t->SetOrientation(glm::normalize(glm::quat_cast(rotMat)));
        t->UpdateMatrix();
    }

    // Mirror FOV and clip planes from the legacy camera.
    auto& cam        = cameraGO->AddComponent<CCamera>();
    cam.isMainCamera = true;
    cam.fov          = gameCamera->GetVerticalFOV();   // degrees
    cam.nearPlane    = gameCamera->GetNearPlane();
    cam.farPlane     = gameCamera->GetFarPlane();
    cam.aspectRatio  = gameCamera->GetAspectRatio();
}

void ModuleScene::RefreshPrefabInstances()
{
    if (!activeScene) return;

    // Phase 1: collect prefab roots from a snapshot.
    // We must NOT call ReloadPrefabInstance while iterating — reload destroys
    // children that are also in the snapshot, leaving dangling pointers.
    NOUS_Vector<GameObject*> prefabRoots(MemoryTag::SCENE);
    {
        const auto snapshot = activeScene->GetGameObjectsSnapshot();
        for (auto* go : snapshot)
        {
            if (go->HasComponent<CPrefab>())
                prefabRoots.push_back(go);
        }
    }

    if (prefabRoots.empty())
    {
        NOUS_INFO("[Scene] RefreshPrefabInstances: no prefab instances found in scene.");
        return;
    }

    NOUS_INFO("[Scene] RefreshPrefabInstances: refreshing %zu prefab instance(s).", prefabRoots.size());

    // Phase 2: reload each root now that the snapshot is discarded.
    for (auto* root : prefabRoots)
        PrefabManager::ReloadPrefabInstance(root, activeScene);

    NOUS_INFO("[Scene] RefreshPrefabInstances: done.");
}
