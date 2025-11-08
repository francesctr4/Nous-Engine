#include "Engine/Modules/ModuleScene/include/ModuleScene.h"
#include "Engine/Modules/ModuleInput/include/ModuleInput.h"
#include "Engine/Modules/ModuleResourceManager/include/ModuleResourceManager.h"
#include "Engine/Core/Event System/EventSystem.h"

#include "Engine/Systems/Resource Manager/Resource Types/ResourceMesh.h"
#include "Engine/Systems/Resource Manager/Resource Types/ResourceMaterial.h"
#include "Engine/Core/Application.h"
#include "Engine/Core/Memory Manager/MemoryManager.h"
#include "Engine/Systems/Camera System/Camera.h"

#include "Engine/Scripting/ScriptManager.h"
#include "Engine/Scripting/Internal/IScript.inl"

#include "Engine/Systems/ECS/Scene.h"
#include "Engine/Systems/ECS/GameObject.h"

#include "Engine/Systems/ECS/Components/ComponentMesh.h"
#include "Engine/Systems/ECS/Components/ComponentMaterial.h"
#include "Engine/NOUS_Multithreading/NOUS_JobSystem/include/NOUS_JobSystem.h"
#include "Engine/NOUS_Multithreading/NOUS_Thread/include/NOUS_Thread.h"

#include "SDL3/SDL.h"
#include <filesystem>

ModuleScene::ModuleScene(Application* app) : Module(app)
{
	NOUS_TRACE("%s()", __FUNCTION__);

	activeScene = NOUS_NEW<Scene>(MemoryManager::MemoryTag::GAME);
	gameCamera = NOUS_NEW<Camera>(MemoryManager::MemoryTag::GAME);
	scriptManager = NOUS_NEW<ScriptManager>(MemoryManager::MemoryTag::GAME);

	// Load the script library
	if (!scriptManager->LoadScriptLibrary("Scripts/Scripts.dll")) {
		NOUS_ERROR("Failed to load script library on startup");
	}

	// Create script instances
	CreateScriptInstances();

	App->eventSystem->Subscribe(EventType::WINDOW_RESIZED, this);
}

ModuleScene::~ModuleScene()
{
	NOUS_TRACE("%s()", __FUNCTION__);

    selectedGameObject = nullptr;

	// Clean up scripts before destroying script manager
	CleanupScripts();

	NOUS_DELETE<Camera>(gameCamera, MemoryManager::MemoryTag::GAME);
	NOUS_DELETE<ScriptManager>(scriptManager, MemoryManager::MemoryTag::GAME);
	NOUS_DELETE<Scene>(activeScene, MemoryManager::MemoryTag::GAME);
}

bool ModuleScene::Awake()
{
	NOUS_TRACE("%s()", __FUNCTION__);

	gameCamera->SetPos(-4.61f, 100.0f, 718.32f);

	for (auto& script : scripts)
	{
		if (script) {
			script->Awake();
		}
	}

	return true;
}

bool ModuleScene::Start()
{
	NOUS_TRACE("%s()", __FUNCTION__);

	for (auto& script : scripts)
	{
		if (script) {
			script->Start();
		}
	}

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

	// Update only valid scripts
	for (auto it = scripts.begin(); it != scripts.end(); ) {
		if (*it) {
			(*it)->Update(dt);
			++it;
		} else {
			// Remove null scripts
			it = scripts.erase(it);
		}
	}

	if (App->input->GetKey(SDL_SCANCODE_M) == KeyState::DOWN)
	{
		ScriptManager::GenerateScript("PRUEBA_CREAR_SCRIPT_DESDE_MOTOR");
	}

    if (App->input->GetKey(SDL_SCANCODE_Z) == KeyState::DOWN)
    {
        SaveScene("../../../Assets/Scenes/LagiacrusScene.nous");
    }

	if (App->input->GetKey(SDL_SCANCODE_X) == KeyState::DOWN)
	{
		ClearScene();
	}

	if (App->input->GetKey(SDL_SCANCODE_C) == KeyState::DOWN)
	{
		LoadScene("../../../Assets/Scenes/LagiacrusScene.nous");
	}

    if (App->input->GetKey(SDL_SCANCODE_F1) == KeyState::DOWN)
    {
        App->jobSystem->SubmitJob([this]()
                                  {
                                      GameObject* go = activeScene->CreateGameObject("Lagiacrus Head");

                                      auto& meshComp = go->AddComponent<CMesh>();
                                      meshComp.mesh = down_cast<ResourceMesh*>(App->resourceManager->CreateResource("Assets/Meshes/Lagiacrus_Head.fbx"));

//                                      auto& matComp = go->AddComponent<CMaterial>();
//                                      matComp.material = down_cast<ResourceMaterial*>(App->resourceManager->CreateResource("Assets/Materials/Lagiacrus_Head.nmat"));

                                  }, "Render Lagiacrus");
    }

    if (App->input->GetKey(SDL_SCANCODE_F2) == KeyState::DOWN)
    {
        App->jobSystem->SubmitJob([this]()
                                  {
                                      GameObject* go = activeScene->CreateGameObject("Cypher");

                                      auto& meshComp = go->AddComponent<CMesh>();
                                      meshComp.mesh = down_cast<ResourceMesh*>(App->resourceManager->CreateResource("Assets/Meshes/Cypher_S0_Skelmesh.fbx"));

                                      auto& matComp = go->AddComponent<CMaterial>();
                                      matComp.material = down_cast<ResourceMaterial*>(App->resourceManager->CreateResource("Assets/Materials/cypher_material.nmat"));

                                  }, "Render Cypher");
    }

    if (App->input->GetKey(SDL_SCANCODE_F3) == KeyState::DOWN)
    {
        App->jobSystem->SubmitJob([this]()
                                  {
                                      GameObject* go = activeScene->CreateGameObject("Queen Xenomorph");

                                      auto& meshComp = go->AddComponent<CMesh>();
                                      meshComp.mesh = down_cast<ResourceMesh*>(App->resourceManager->CreateResource("Assets/Meshes/Queen_Xenomorph.fbx"));

                                      auto& matComp = go->AddComponent<CMaterial>();
                                      matComp.material = down_cast<ResourceMaterial*>(App->resourceManager->CreateResource("Assets/Materials/queen_xenomorph.nmat"));

                                  }, "Render Queen Xenomorph");
    }

    if (App->input->GetKey(SDL_SCANCODE_F4) == KeyState::DOWN)
    {
        App->jobSystem->SubmitJob([this]()
                                  {
                                      GameObject* go = activeScene->CreateGameObject("Wolf");

                                      auto& meshComp = go->AddComponent<CMesh>();
                                      meshComp.mesh = down_cast<ResourceMesh*>(App->resourceManager->CreateResource("Assets/Meshes/Wolf.obj"));

                                      auto& matComp = go->AddComponent<CMaterial>();
                                      matComp.material = down_cast<ResourceMaterial*>(App->resourceManager->CreateResource("Assets/Materials/wolf_material.nmat"));

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
                                          GameObject* go = activeScene->CreateGameObject(model.name);

                                          auto& meshComp = go->AddComponent<CMesh>();
                                          meshComp.mesh = down_cast<ResourceMesh*>(App->resourceManager->CreateResource(model.meshPath));

                                          NOUS_Multithreading::NOUS_Thread::SleepMS(1000); // optional delay

                                          auto& matComp = go->AddComponent<CMaterial>();
                                          matComp.material = down_cast<ResourceMaterial*>(App->resourceManager->CreateResource(model.matPath));

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

	for (auto& script : scripts)
	{
		if (script) {
			script->LateUpdate(dt);
		}
	}

	return UpdateStatus::CONTINUE;
}

bool ModuleScene::CleanUp()
{
	NOUS_TRACE("%s()", __FUNCTION__);

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

			gameCamera->SetAspectRatio((float)event.ctx.i32[0] / (float)event.ctx.i32[1]);

			break;
		}
	}
}

void ModuleScene::CreateScriptInstances() {
    // Clear any existing scripts first
    CleanupScripts();

    // Define the scripts directory path
    std::string scriptsPath = "../../../Assets/Scripts/";

    // Vector to store script names
    std::vector<std::string> scriptNames;

    try {
        // Check if directory exists
        if (!std::filesystem::exists(scriptsPath)) {
            NOUS_WARN("Scripts directory does not exist: %s", scriptsPath.c_str());
            return;
        }

        // Iterate through all files in the directory
        for (const auto& entry : std::filesystem::directory_iterator(scriptsPath)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                std::string extension = entry.path().extension().string();

                // Only process .cpp files (or adjust for your script extension)
                if (extension == ".cpp" || extension == ".inl" || extension == ".h") {
                    // Remove extension to get the class name
                    std::string className = entry.path().stem().string();

                    // Skip the template file if it exists
                    if (className != "ScriptTemplate") {
                        scriptNames.push_back(className);
                    }
                }
            }
        }

        // Create script instances for all found scripts
        for (const std::string& name : scriptNames) {
            IScript* script = scriptManager->CreateScriptInstance(name);
            if (script) {
                scripts.emplace_back(script);
                NOUS_INFO("Created script instance: %s", name.c_str());
            } else {
                NOUS_WARN("Failed to create script instance: %s", name.c_str());
            }
        }

        NOUS_INFO("Successfully created %zu script instances", scriptNames.size());

    } catch (const std::filesystem::filesystem_error& ex) {
        NOUS_ERROR("Filesystem error while reading scripts directory: %s", ex.what());
    } catch (const std::exception& ex) {
        NOUS_ERROR("Error reading scripts directory: %s", ex.what());
    }
}

void ModuleScene::RecompileScripts()
{
	// Clean up current scripts
	CleanupScripts();

	// Reload the script library
	if (scriptManager->ReloadScriptLibrary("Scripts/Scripts.dll")) {
		// Recreate script instances
		CreateScriptInstances();

		// Re-initialize scripts
		for (auto &script: scripts) {
			if (script) {
				script->Awake();
				script->Start();
			}
		}
		NOUS_INFO("Script hot-reload completed successfully");
	} else {
		NOUS_ERROR("Script hot-reload failed");
	}
}


void ModuleScene::CleanupScripts() {
	// Call cleanup on all scripts first
	for (auto& script : scripts) {
		if (script) {
			// Note: If your IScript has a cleanup/destructor method, call it here
			delete script;
		}
	}
	scripts.clear();
	NOUS_INFO("Cleaned up all script instances");
}

void ModuleScene::SaveScene(const std::string& path)
{
    activeScene->Serialize(path);
}

void ModuleScene::LoadScene(const std::string& path)
{
	ClearScene();

	App->jobSystem->SubmitJob([this, path](){
		activeScene->Deserialize(path);
	}, "LoadScene");

}

void ModuleScene::ClearScene()
{
    selectedGameObject = nullptr;
    activeScene->Clear();
}
