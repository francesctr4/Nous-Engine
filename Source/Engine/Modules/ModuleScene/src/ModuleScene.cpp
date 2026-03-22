#include "Engine/Modules/ModuleScene/include/ModuleScene.h"
#include "Engine/Modules/ModuleInput/include/ModuleInput.h"
#include "Engine/Modules/ModuleResourceManager/include/ModuleResourceManager.h"
#include "Engine/Core/EventSystem/EventSystem.h"
#include "Engine/Core/Logger/Logger.h"

#include "Engine/Systems/ResourceManager/Resource/ResourceMesh/include/ResourceMesh.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceMaterial/include/ResourceMaterial.h"
#include "Engine/Core/Application.h"
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
#include "Engine/NOUS_Multithreading/NOUS_JobSystem/include/NOUS_JobSystem.h"
#include "Engine/NOUS_Multithreading/NOUS_Thread/include/NOUS_Thread.h"

#include "Engine/Core/TimeManager/TimeManager.h"

#include <SDL3/SDL.h>
#include <filesystem>
#include <algorithm>

#include "Engine/Systems/ResourceManager/Resource/ResourceShader/include/ResourceShader.h"

ModuleScene::ModuleScene(Application* app)
    : Module(app), m_scriptComponents(MemoryTag::SCRIPTING_SYSTEM)
{
	activeScene   = NOUS_NEW<Scene>(MemoryTag::SCENE);
	gameCamera    = NOUS_NEW<Camera>(MemoryTag::CAMERA);
	scriptManager = NOUS_NEW<ScriptManager>(MemoryTag::SCRIPTING_SYSTEM);

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
        (std::filesystem::path(SDL_GetBasePath()) / "Scripts" / kScriptsLib).string();
	if (!scriptManager->LoadScriptLibrary(scriptsDllPath))
		NOUS_ERROR("Failed to load script library on startup");

	App->eventSystem->Subscribe(EventType::WINDOW_RESIZED, this);
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
		m_pendingStop = false;
		LoadScene(m_snapshotPath);
		NOUS_INFO("[Scene] Simulation stopped — scene restored from snapshot.");
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

	if (App->input->GetKey(SDL_SCANCODE_M) == KeyState::DOWN)
	{
		ScriptManager::GenerateScript("PRUEBA_CREAR_SCRIPT_DESDE_MOTOR");
	}

    if (App->input->GetKey(SDL_SCANCODE_Z) == KeyState::DOWN)
    {
        SaveScene("Assets/Scenes/LagiacrusScene.nous");
    }

	if (App->input->GetKey(SDL_SCANCODE_X) == KeyState::DOWN)
	{
		ClearScene();
	}

	if (App->input->GetKey(SDL_SCANCODE_C) == KeyState::DOWN)
	{
		LoadScene("Assets/Scenes/LagiacrusScene.nous");
	}

    if (App->input->GetKey(SDL_SCANCODE_F1) == KeyState::DOWN)
    {
        App->jobSystem->SubmitJob([this]()
                                  {
                                      GameObject* go = activeScene->CreateGameObjectDetached("Lagiacrus Head");

                                      auto& meshComp = go->AddComponent<CMesh>();
                                      meshComp.mesh = down_cast<ResourceMesh*>(App->resourceManager->CreateResource("Assets/Meshes/Lagiacrus_Head.fbx"));

//                                      auto& matComp = go->AddComponent<CMaterial>();
//                                      matComp.material = down_cast<ResourceMaterial*>(App->resourceManager->CreateResource("Assets/Materials/Lagiacrus_Head.nmat"));

                                      activeScene->RegisterGameObject(go);
                                  }, "Render Lagiacrus");
    }

    if (App->input->GetKey(SDL_SCANCODE_F2) == KeyState::DOWN)
    {
        App->jobSystem->SubmitJob([this]()
                                  {
                                      GameObject* go = activeScene->CreateGameObjectDetached("Cypher");

                                      auto& meshComp = go->AddComponent<CMesh>();
                                      meshComp.mesh = down_cast<ResourceMesh*>(App->resourceManager->CreateResource("Assets/Meshes/Cypher_S0_Skelmesh.fbx"));

                                      auto& matComp = go->AddComponent<CMaterial>();
                                      matComp.material = down_cast<ResourceMaterial*>(App->resourceManager->CreateResource("Assets/Materials/cypher_material.nmat"));

                                      activeScene->RegisterGameObject(go);
                                  }, "Render Cypher");
    }

    if (App->input->GetKey(SDL_SCANCODE_F3) == KeyState::DOWN)
    {
        App->jobSystem->SubmitJob([this]()
                                  {
                                      GameObject* go = activeScene->CreateGameObjectDetached("Queen Xenomorph");

                                      auto& meshComp = go->AddComponent<CMesh>();
                                      meshComp.mesh = down_cast<ResourceMesh*>(App->resourceManager->CreateResource("Assets/Meshes/Queen_Xenomorph.fbx"));

                                      auto& matComp = go->AddComponent<CMaterial>();
                                      matComp.material = down_cast<ResourceMaterial*>(App->resourceManager->CreateResource("Assets/Materials/queen_xenomorph.nmat"));

                                      activeScene->RegisterGameObject(go);
                                  }, "Render Queen Xenomorph");
    }

    if (App->input->GetKey(SDL_SCANCODE_F4) == KeyState::DOWN)
    {
        App->jobSystem->SubmitJob([this]()
                                  {
                                      GameObject* go = activeScene->CreateGameObjectDetached("Wolf");

                                      auto& meshComp = go->AddComponent<CMesh>();
                                      meshComp.mesh = down_cast<ResourceMesh*>(App->resourceManager->CreateResource("Assets/Meshes/Wolf.obj"));

                                      auto& matComp = go->AddComponent<CMaterial>();
                                      matComp.material = down_cast<ResourceMaterial*>(App->resourceManager->CreateResource("Assets/Materials/wolf_material.nmat"));

                                      activeScene->RegisterGameObject(go);
                                  }, "Render Wolf");
    }

// Optional: Batch creation with delays (like your F5 example)
    if (App->input->GetKey(SDL_SCANCODE_F5) == KeyState::DOWN)
    {
        struct ModelData { const char* name; const char* meshPath; const char* matPath; const char* jobName; };
        std::vector<ModelData> models = {
                {"Lagiacrus Head", "Assets/Meshes/Lagiacrus_Head.fbx", "Assets/Materials/Lagiacrus_Head.nmat", "Render Lagiacrus"},
                {"Cypher", "Assets/Meshes/Cypher_S0_Skelmesh.fbx", "Assets/Materials/cypher_material.nmat", "Render Cypher"},
                {"Queen Xenomorph", "Assets/Meshes/Queen_Xenomorph.fbx", "Assets/Materials/queen_xenomorph.nmat", "Render Queen Xenomorph"},
                {"Wolf", "Assets/Meshes/Wolf.obj", "Assets/Materials/wolf_material.nmat", "Render Wolf"}
        };

        for (const auto& model : models)
        {
            App->jobSystem->SubmitJob([this, model]()
                                      {
                                          GameObject* go = activeScene->CreateGameObjectDetached(model.name);

                                          auto& meshComp = go->AddComponent<CMesh>();
                                          meshComp.mesh = down_cast<ResourceMesh*>(App->resourceManager->CreateResource(model.meshPath));

                                          auto& matComp = go->AddComponent<CMaterial>();
                                          matComp.material = down_cast<ResourceMaterial*>(App->resourceManager->CreateResource(model.matPath));

                                          activeScene->RegisterGameObject(go);
                                      }, model.jobName);
        }
    }

	if (App->input->GetKey(SDL_SCANCODE_F6) == KeyState::DOWN)
	{
		ClearScene();
	}

	if (App->input->GetKey(SDL_SCANCODE_F7) == KeyState::DOWN)
	{
		App->jobSystem->SubmitJob([this]()
								  {
									  NOUS_Multithreading::NOUS_Thread::SleepMS(5000);
								  }, "Test");
	}

	if (App->input->GetKey(SDL_SCANCODE_F8) == KeyState::DOWN)
	{
		for (int i = 0; i < 100; ++i)
		{
			App->jobSystem->SubmitJob([]
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

	if (App->input->GetKey(SDL_SCANCODE_F9) == KeyState::DOWN)
	{
		NOUS_INFO("Initiating script hot-reload...");

		App->jobSystem->SubmitJob([this]
		{
			RecompileScripts();

		}, "Scripts Hot-Reload");
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

	return UpdateStatus::CONTINUE;
}

bool ModuleScene::CleanUp()
{
	// Wait for any in-flight jobs (e.g. hot-reload) before touching scripts
	App->jobSystem->WaitForPendingJobs();

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

void ModuleScene::RecompileScripts()
{
	// Phase 1: destroy all DLL-allocated instances but keep the component names.
	// Lock is released afterward so the main thread keeps rendering harmlessly
	// (CScript::OnUpdate iterates an empty m_instances — safe no-op).
	{
		std::lock_guard<std::mutex> lock(m_scriptComponentsMutex);
		for (auto* cs : m_scriptComponents)
			if (cs) cs->ClearInstances();
	}

	// Phase 2: rebuild DLL (lock not held)
	const std::string dllPath =
        (std::filesystem::path(SDL_GetBasePath()) / "Scripts" / "Scripts.dll").string();

	if (!scriptManager->ReloadScriptLibrary(dllPath))
	{
		NOUS_ERROR("Script hot-reload failed");
		return;
	}

	// Phase 3: recreate instances from the new DLL
	{
		std::lock_guard<std::mutex> lock(m_scriptComponentsMutex);
		for (auto* cs : m_scriptComponents)
			if (cs) cs->RecreateInstances();
	}

	NOUS_INFO("Script hot-reload completed successfully");
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
	App->jobSystem->WaitForPendingJobs();

	// Ensure the snapshot directory exists, then save the current scene state.
	std::filesystem::create_directories(std::filesystem::path(m_snapshotPath).parent_path());
	activeScene->Serialize(m_snapshotPath);

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
}

void ModuleScene::LoadScene(const std::string& path)
{
	// Drain any in-flight jobs (e.g. debug hotkey loaders) before clearing the
	// scene. Without this, a job that called CreateGameObjectDetached before the
	// clear could still call RegisterGameObject afterward on a cleared scene.
	App->jobSystem->WaitForPendingJobs();

	ClearScene();
	activeScene->Deserialize(path);
	EnsureMainCamera();
}

void ModuleScene::LoadSceneAsync(const std::string& path)
{
	// Drain any in-flight jobs (e.g. debug hotkey loaders) before clearing the
	// scene. Without this, a job that called CreateGameObjectDetached before the
	// clear could still call RegisterGameObject afterward on a cleared scene.
	App->jobSystem->WaitForPendingJobs();

	ClearScene();

	App->jobSystem->SubmitJob([this, path]
		{
			activeScene->Deserialize(path);
			EnsureMainCamera();
		}
	);
}

void ModuleScene::ClearScene()
{
    selectedGameObject = nullptr;
    activeScene->Clear();
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
