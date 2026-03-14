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

#include <SDL3/SDL.h>
#include <filesystem>
#include <algorithm>

#include "Engine/Systems/ResourceManager/Resource/ResourceShader/include/ResourceShader.h"

ModuleScene::ModuleScene(Application* app)
    : Module(app), m_scriptComponents(MemoryTag::SCRIPTING_SYSTEM)
{
	NOUS_TRACE("%s()", __FUNCTION__);

	activeScene   = NOUS_NEW<Scene>(MemoryTag::SCENE);
	gameCamera    = NOUS_NEW<Camera>(MemoryTag::CAMERA);
	scriptManager = NOUS_NEW<ScriptManager>(MemoryTag::SCRIPTING_SYSTEM);

	// Load the script library — path is exe-relative so it works regardless of working directory.
	// SDL3's SDL_GetBasePath() returns a const char* managed internally by SDL — do NOT free it.
	const std::string scriptsDllPath =
        (std::filesystem::path(SDL_GetBasePath()) / "Scripts" / "Scripts.dll").string();
	if (!scriptManager->LoadScriptLibrary(scriptsDllPath))
		NOUS_ERROR("Failed to load script library on startup");

	App->eventSystem->Subscribe(EventType::WINDOW_RESIZED, this);
}

ModuleScene::~ModuleScene()
{
	NOUS_TRACE("%s()", __FUNCTION__);

    selectedGameObject = nullptr;

	NOUS_DELETE(gameCamera, MemoryTag::CAMERA);
	NOUS_DELETE(scriptManager, MemoryTag::SCRIPTING_SYSTEM);
	NOUS_DELETE(activeScene, MemoryTag::SCENE);
}

bool ModuleScene::Awake()
{
	NOUS_TRACE("%s()", __FUNCTION__);

	gameCamera->SetPos(-4.61f, 100.0f, 718.32f);

	return true;
}

bool ModuleScene::Start()
{
	NOUS_TRACE("%s()", __FUNCTION__);

    LoadScene("Assets/Scenes/LagiacrusScene.nous");

	return true;
}

UpdateStatus ModuleScene::PreUpdate(float dt)
{
	NOUS_TRACE("%s()", __FUNCTION__);
	return UpdateStatus::CONTINUE;
}

UpdateStatus ModuleScene::Update(float dt)
{
	NOUS_TRACE("%s()", __FUNCTION__);

	// Update all scene GameObjects — calls OnUpdate on each component (CCamera, CScript, …)
	if (activeScene)
		activeScene->Update(dt);

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
											  std::sqrt(123.456); // Dummy CPU-bound work
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
	NOUS_TRACE("%s()", __FUNCTION__);

	// Dispatch LateUpdate to all CScript components
	{
		std::lock_guard<std::mutex> lock(m_scriptComponentsMutex);
		for (auto* cs : m_scriptComponents)
			if (cs) cs->LateUpdate(dt);
	}

	return UpdateStatus::CONTINUE;
}

bool ModuleScene::CleanUp()
{
	NOUS_TRACE("%s()", __FUNCTION__);

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
			NOUS_DEBUG("%s() --> WINDOW RESIZED EVENT", __FUNCTION__);
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

	// Destroy DLL-allocated instances; ClearInstances sets m_started=false so
	// the subsequent CScript::OnDestroy (from scene destruction) skips unregister.
	for (auto* cs : m_scriptComponents)
		if (cs) cs->ClearInstances();

	m_scriptComponents.clear();
	NOUS_INFO("Cleaned up all CScript instances");
}

void ModuleScene::SaveScene(const std::string& path)
{
    activeScene->Serialize(path);
}

void ModuleScene::LoadScene(const std::string& path)
{
	// Drain any in-flight mesh/material loading jobs before clearing the scene.
	// Without this, a job that called CreateGameObjectDetached before the clear
	// could still call RegisterGameObject afterward, inserting a stale GO into
	// the freshly loaded scene (or crashing if ClearScene already deleted it).
	App->jobSystem->WaitForPendingJobs();

	ClearScene();

	App->jobSystem->SubmitJob([this, path](){
		activeScene->Deserialize(path);
		EnsureMainCamera();
	}, "LoadScene");
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
