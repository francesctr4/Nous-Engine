#include "Application.h"
#include "Module.h"

#include "ModuleWindow.h"
#include "ModuleInput.h"
#include "ModuleCamera3D.h"
#include "ModuleRenderer3D.h"

#include "Logger.h"

extern Application* External = nullptr;

Application::Application()
{
	External = this;

    targetFPS = 1.0f / 60.0f;

	window = new ModuleWindow(this, "ModuleWindow");
	input = new ModuleInput(this, "ModuleInput");
    camera = new ModuleCamera3D(this, "ModuleCamera3D");
    renderer = new ModuleRenderer3D(this, "ModuleRenderer3D");

	list_modules[0] = window;
	list_modules[1] = input;
    list_modules[2] = camera;
    list_modules[3] = renderer;
}

Application::~Application()
{
	for (int i = 0; i < NUM_MODULES; ++i)
	{
		if (list_modules[i] != nullptr) {
			delete list_modules[i];
			list_modules[i] = nullptr;
		}
	}
}

bool Application::Awake()
{
    bool ret = true;

    // Call Awake() in all modules
    for (int i = 0; i < NUM_MODULES && ret; ++i)
    {
        if (list_modules[i] != nullptr) {
            ret = list_modules[i]->Awake();
        }
    }

    // After all Awake calls we call Start() in all modules
    NOUS_INFO("-------------- Application Start --------------");
    for (int i = 0; i < NUM_MODULES && ret; ++i)
    {
        if (list_modules[i] != nullptr) {
            ret = list_modules[i]->Start();
        }
    }

    return ret;
}

UpdateStatus Application::Update()
{
    UpdateStatus ret = UPDATE_CONTINUE;
    PrepareUpdate();

    for (int i = 0; i < NUM_MODULES && ret == UPDATE_CONTINUE; ++i)
    {
        if (list_modules[i] != nullptr) {
            ret = list_modules[i]->PreUpdate((targetFPS));
        }
    }

    for (int i = 0; i < NUM_MODULES && ret == UPDATE_CONTINUE; ++i)
    {
        if (list_modules[i] != nullptr) {
            ret = list_modules[i]->Update(targetFPS);
        }
    }

    for (int i = 0; i < NUM_MODULES && ret == UPDATE_CONTINUE; ++i)
    {
        if (list_modules[i] != nullptr) {
            ret = list_modules[i]->PostUpdate(targetFPS);
        }
    }

    FinishUpdate();
    return ret;
}

bool Application::CleanUp()
{
    bool ret = true;
    for (int i = (NUM_MODULES - 1); i >= 0 && ret; --i)
    {
        if (list_modules[i] != nullptr) {
            ret = list_modules[i]->CleanUp();
        }
    }
    return ret;
}

void Application::PrepareUpdate()
{

}

void Application::FinishUpdate()
{

}