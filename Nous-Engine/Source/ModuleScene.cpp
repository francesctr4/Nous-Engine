#include "ModuleScene.h"
#include "ModuleInput.h"
#include "ModuleResourceManager.h"

#include "ResourceMesh.h"
#include "ResourceMaterial.h"

#include "MemoryManager.h"

ModuleScene::ModuleScene(Application* app) : Module(app)
{
	NOUS_TRACE("%s()", __FUNCTION__);

	gameCamera = NOUS_NEW<Camera>(MemoryManager::MemoryTag::GAME);
}

ModuleScene::~ModuleScene()
{
	NOUS_TRACE("%s()", __FUNCTION__);

	NOUS_DELETE<Camera>(gameCamera, MemoryManager::MemoryTag::GAME);
}

bool ModuleScene::Awake()
{
	NOUS_TRACE("%s()", __FUNCTION__);

	gameCamera->SetPos(-4.61, 100, 718.32);

	return true;
}

bool ModuleScene::Start()
{
	NOUS_TRACE("%s()", __FUNCTION__);
	return true;
}

UpdateStatus ModuleScene::PreUpdate(float dt)
{
	NOUS_TRACE("%s()", __FUNCTION__);
	return UPDATE_CONTINUE;
}

UpdateStatus ModuleScene::Update(float dt)
{
	NOUS_TRACE("%s()", __FUNCTION__);

	if (App->input->GetKey(SDL_SCANCODE_H) == KeyState::DOWN)
	{
		App->jobSystem->SubmitJob([this]()
			{
				NOUS_Multithreading::NOUS_Thread::SleepMS(10000);
			}, "Test");
	}

	if (App->input->GetKey(SDL_SCANCODE_T) == KeyState::DOWN) 
	{
		App->jobSystem->SubmitJob([this]()
			{
				ResourceMesh* mesh2 = static_cast<ResourceMesh*>(App->resourceManager->CreateResource("Assets/Meshes/Lagiacrus_Head.fbx"));
				mesh2->material = static_cast<ResourceMaterial*>(App->resourceManager->CreateResource("Assets/Materials/Lagiacrus_Head.nmat"));
			}, "Render Lagiacrus");
	}

	if (App->input->GetKey(SDL_SCANCODE_G) == KeyState::DOWN)
	{
		App->jobSystem->SubmitJob([this]()
			{
				ResourceMesh* mesh2 = static_cast<ResourceMesh*>(App->resourceManager->CreateResource("Assets/Meshes/Cypher_S0_Skelmesh.fbx"));
				mesh2->material = static_cast<ResourceMaterial*>(App->resourceManager->CreateResource("Assets/Materials/cypher_material.nmat"));
			}, "Render Cypher");
	}

	if (App->input->GetKey(SDL_SCANCODE_B) == KeyState::DOWN)
	{
		App->jobSystem->SubmitJob([this]()
			{
				ResourceMesh* mesh2 = static_cast<ResourceMesh*>(App->resourceManager->CreateResource("Assets/Meshes/Queen_Xenomorph.fbx"));
				mesh2->material = static_cast<ResourceMaterial*>(App->resourceManager->CreateResource("Assets/Materials/queen_xenomorph.nmat"));
			}, "Render Queen Xenomorph");
	}

	if (App->input->GetKey(SDL_SCANCODE_N) == KeyState::DOWN)
	{
		App->jobSystem->SubmitJob([this]()
			{
				ResourceMesh* mesh2 = static_cast<ResourceMesh*>(App->resourceManager->CreateResource("Assets/Meshes/Wolf.obj"));
				mesh2->material = static_cast<ResourceMaterial*>(App->resourceManager->CreateResource("Assets/Materials/wolf_material.nmat"));
			}, "Render Wolf");
	}

	if (App->input->GetKey(SDL_SCANCODE_M) == KeyState::DOWN) 
	{
		App->jobSystem->SubmitJob([this]()
			{
				NOUS_Multithreading::NOUS_Thread::SleepMS(1000);
				ResourceMesh* mesh2 = static_cast<ResourceMesh*>(App->resourceManager->CreateResource("Assets/Meshes/Lagiacrus_Head.fbx"));
				NOUS_Multithreading::NOUS_Thread::SleepMS(1000);
				mesh2->material = static_cast<ResourceMaterial*>(App->resourceManager->CreateResource("Assets/Materials/Lagiacrus_Head.nmat"));
			}, "Render Lagiacrus");

		App->jobSystem->SubmitJob([this]()
			{
				NOUS_Multithreading::NOUS_Thread::SleepMS(1000);
				ResourceMesh* mesh2 = static_cast<ResourceMesh*>(App->resourceManager->CreateResource("Assets/Meshes/Cypher_S0_Skelmesh.fbx"));
				NOUS_Multithreading::NOUS_Thread::SleepMS(1000);
				mesh2->material = static_cast<ResourceMaterial*>(App->resourceManager->CreateResource("Assets/Materials/cypher_material.nmat"));
			}, "Render Cypher");

		App->jobSystem->SubmitJob([this]()
			{
				NOUS_Multithreading::NOUS_Thread::SleepMS(1000);
				ResourceMesh* mesh2 = static_cast<ResourceMesh*>(App->resourceManager->CreateResource("Assets/Meshes/Queen_Xenomorph.fbx"));
				NOUS_Multithreading::NOUS_Thread::SleepMS(1000);
				mesh2->material = static_cast<ResourceMaterial*>(App->resourceManager->CreateResource("Assets/Materials/queen_xenomorph.nmat"));
			}, "Render Queen Xenomorph");

		App->jobSystem->SubmitJob([this]()
			{
				NOUS_Multithreading::NOUS_Thread::SleepMS(1000);
				ResourceMesh* mesh2 = static_cast<ResourceMesh*>(App->resourceManager->CreateResource("Assets/Meshes/Wolf.obj"));
				NOUS_Multithreading::NOUS_Thread::SleepMS(1000);
				mesh2->material = static_cast<ResourceMaterial*>(App->resourceManager->CreateResource("Assets/Materials/wolf_material.nmat"));
			}, "Render Wolf");
	}

	return UPDATE_CONTINUE;
}

UpdateStatus ModuleScene::PostUpdate(float dt)
{
	NOUS_TRACE("%s()", __FUNCTION__);
	return UPDATE_CONTINUE;
}

bool ModuleScene::CleanUp()
{
	NOUS_TRACE("%s()", __FUNCTION__);

	return true;
}

void ModuleScene::ReceiveEvent(const Event& event)
{
	switch (event.type)
	{
		case EventType::WINDOW_RESIZED:
		{
			NOUS_DEBUG("%s() --> WINDOW RESIZED EVENT", __FUNCTION__);
			NOUS_DEBUG("Received context: %d, %d", event.context.int64[0], event.context.int64[1]);

			gameCamera->SetAspectRatio((float)event.context.int64[0] / (float)event.context.int64[1]);

			break;
		}
	}
}