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
				NOUS_Multithreading::NOUS_Thread::SleepMS(2000);
				ResourceMesh* mesh2 = static_cast<ResourceMesh*>(App->resourceManager->CreateResource("Assets/Meshes/Lagiacrus_Head.fbx"));
				NOUS_Multithreading::NOUS_Thread::SleepMS(2000);
				mesh2->material = static_cast<ResourceMaterial*>(App->resourceManager->CreateResource("Assets/Materials/Lagiacrus_Head.nmat"));
				NOUS_Multithreading::NOUS_Thread::SleepMS(2000);

			}, "Render Lagiacrus");
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