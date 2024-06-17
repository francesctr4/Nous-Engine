#include "Application.h"

#include "Module.h"
#include "ModuleWindow.h"
#include "ModuleInput.h"
#include "Logger.h"

extern Application* External = nullptr;

Application::Application()
{
	External = this;
	window = new ModuleWindow(this);
	input = new ModuleInput(this);

	list_modules[0] = window;
	list_modules[1] = input;
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

    // Call Init() in all modules
    for (int i = 0; i < NUM_MODULES && ret; ++i)
    {
        if (list_modules[i] != nullptr) {
            ret = list_modules[i]->Awake();
        }
    }

    // After all Init calls we call Start() in all modules
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
            ret = list_modules[i]->PreUpdate((1 / 60));
        }
    }

    for (int i = 0; i < NUM_MODULES && ret == UPDATE_CONTINUE; ++i)
    {
        if (list_modules[i] != nullptr) {
            ret = list_modules[i]->Update((1 / 60));
        }
    }

    for (int i = 0; i < NUM_MODULES && ret == UPDATE_CONTINUE; ++i)
    {
        if (list_modules[i] != nullptr) {
            ret = list_modules[i]->PostUpdate((1 / 60));
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

void Application::AddModule(Module* mod)
{

}

void Application::PrepareUpdate()
{

}

void Application::FinishUpdate()
{

}