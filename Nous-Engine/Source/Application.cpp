#include "Application.h"
#include "Module.h"

#include "ModuleWindow.h"
#include "ModuleInput.h"
#include "ModuleCamera3D.h"
#include "ModuleRenderer3D.h"

#include "Logger.h"
#include "ThreadPool.h"
#include "MemoryManager.h"

#include "Tracy.h"

extern Application* External = nullptr;

Application::Application()
{
	External = this;

    targetFPS = 1.0f / 60.0f;

    // We allocate the memory for the module first, then we use it with new to call the constructor

    //void* windowMemory = MemoryManager::Allocate(sizeof(ModuleWindow), MemoryManager::MemoryTag::APPLICATION);
    //window = new(windowMemory) ModuleWindow(this, "ModuleWindow");

    window = NOUS_NEW<ModuleWindow>(MemoryManager::MemoryTag::APPLICATION, this, "ModuleWindow");
    input = NOUS_NEW<ModuleInput>(MemoryManager::MemoryTag::APPLICATION, this, "ModuleInput");
    camera = NOUS_NEW<ModuleCamera3D>(MemoryManager::MemoryTag::APPLICATION, this, "ModuleCamera3D");
    renderer = NOUS_NEW<ModuleRenderer3D>(MemoryManager::MemoryTag::APPLICATION, this, "ModuleRenderer3D");

    /*void* inputMemory = MemoryManager::Allocate(sizeof(ModuleInput), MemoryManager::MemoryTag::APPLICATION);
    input = new(inputMemory) ModuleInput(this, "ModuleInput");

    void* cameraMemory = MemoryManager::Allocate(sizeof(ModuleCamera3D), MemoryManager::MemoryTag::APPLICATION);
    camera = new(cameraMemory) ModuleCamera3D(this, "ModuleCamera3D");

    void* rendererMemory = MemoryManager::Allocate(sizeof(ModuleRenderer3D), MemoryManager::MemoryTag::APPLICATION);
    renderer = new(rendererMemory) ModuleRenderer3D(this, "ModuleRenderer3D");*/

	list_modules[0] = window;
	list_modules[1] = input;
    list_modules[2] = camera;
    list_modules[3] = renderer;
}

Application::~Application()
{
	for (int i = 0; i < NUM_MODULES; ++i)
	{
        NOUS_DELETE(list_modules[i], MemoryManager::MemoryTag::APPLICATION);

		//if (list_modules[i] != nullptr) {

  //          // Call the destructor of the module before deallocating memory
  //          list_modules[i]->~Module(); 

  //          // We use MemoryManager to free the memory
  //          MemoryManager::Free(list_modules[i], sizeof(*list_modules[i]), MemoryManager::MemoryTag::APPLICATION);

  //          list_modules[i] = nullptr;
		//}
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

    NOUS_INFO(MemoryManager::GetMemoryUsageStats());

    return ret;
}

UpdateStatus Application::Update()
{

#ifdef TRACY_ENABLE
    ZoneScoped;
#endif
    
    UpdateStatus ret = UPDATE_CONTINUE;

    ret = PrepareUpdate();

    NOUS_INFO("-------------- PreUpdate --------------");

    for (int i = 0; i < NUM_MODULES && ret == UPDATE_CONTINUE; ++i)
    {
        if (list_modules[i] != nullptr) {
            ret = list_modules[i]->PreUpdate(targetFPS);
        }
    }

    NOUS_INFO("-------------- Update --------------");

    for (int i = 0; i < NUM_MODULES && ret == UPDATE_CONTINUE; ++i)
    {
        if (list_modules[i] != nullptr) {
            ret = list_modules[i]->Update(targetFPS);
        }
    }

    NOUS_INFO("-------------- PostUpdate --------------");

    for (int i = 0; i < NUM_MODULES && ret == UPDATE_CONTINUE; ++i)
    {
        if (list_modules[i] != nullptr) {
            ret = list_modules[i]->PostUpdate(targetFPS);
        }
    }

    FinishUpdate();

#ifdef TRACY_ENABLE
    FrameMark;
#endif

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

UpdateStatus Application::PrepareUpdate()
{
    UpdateStatus ret = UPDATE_CONTINUE;

    return ret;
}

void Application::FinishUpdate()
{

}