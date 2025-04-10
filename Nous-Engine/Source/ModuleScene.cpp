#include "ModuleScene.h"
#include "ModuleInput.h"
#include "ModuleResourceManager.h"

#include "ResourceMesh.h"
#include "ResourceMaterial.h"

ModuleScene::ModuleScene(Application* app, std::string name, bool start_enabled) : Module(app, name, start_enabled)
{
	NOUS_TRACE("%s()", __FUNCTION__);
}

ModuleScene::~ModuleScene()
{
	NOUS_TRACE("%s()", __FUNCTION__);
}

bool ModuleScene::Awake()
{
	NOUS_TRACE("%s()", __FUNCTION__);
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

}