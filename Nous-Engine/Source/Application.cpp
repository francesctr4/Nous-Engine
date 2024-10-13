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

#include "TimeManager.h"

extern Application* External = nullptr;

Application::Application()
{
	External = this;

    targetFPS = 60.0f;

    // We allocate the memory for the module first, then we use it with new to call the constructor.
    // The application itself should NOT use custom allocators.

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

    NOUS_INFO(MemoryManager::GetMemoryUsageStats());

    msTimer.Start();

    TimeManager::frameCount = 0;
    TimeManager::graphicsTimer.Start();

    return ret;
}

UpdateStatus Application::PrepareUpdate()
{
    UpdateStatus ret = UPDATE_CONTINUE;

    //// Measure the time elapsed since the last frame
    //dt = (float)ms_timer.ReadMS() / 1000.0f;
    //ms_timer.Start();

    //const float targetFrameTime = 1.0f / targetFPS;

    //if (dt < targetFrameTime) {

    //    /* If the time elapsed since the last frame is less than the target frame time,
    //    introduce a delay to ensure we wait until the target frame time has elapsed. */

    //    Sleep((targetFrameTime - dt) * 1000); // Convert to milliseconds

    //    dt = targetFrameTime; // Update dt to match the target frame time.
    //}

    //TimeManager::DeltaTime = dt;
    //TimeManager::FrameCount++;

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

void Application::FinishUpdate()
{

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

void Application::BroadcastEvent(const Event& event)
{
    for (int i = 0; i < NUM_MODULES; ++i)
    {
        if (list_modules[i] != nullptr) 
        {
            list_modules[i]->ReceiveEvent(event);
        }
    }
}

void Application::SetTargetFPS(float FPS)
{
    
}

float32 Application::GetTargetFPS()
{
    return targetFPS;
}

float32 Application::GetFPS()
{
    return float32();
}

float32 Application::GetDT()
{
    return float32();
}

float32 Application::GetMS()
{
    return float32();
}