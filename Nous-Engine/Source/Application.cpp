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
#include "SDL2.h"

#include "TimeManager.h"

extern Application* External = nullptr;

Application::Application()
{
	External = this;

    isMinimized = false;

    targetFPS = DEFAULT_TARGET_FPS;
    dt = 0.0f;

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

    updateTitleTimer.Start();

    return ret;
}

UpdateStatus Application::PrepareUpdate()
{
    NOUS_TRACE("%s()", __FUNCTION__);

    UpdateStatus ret = UPDATE_CONTINUE;

    dt = msTimer.ReadSec();
    msTimer.Start();

    TimeManager::deltaTime = dt;
    TimeManager::frameCount++;

    return ret;
}

UpdateStatus Application::Update()
{
    UpdateStatus ret = UPDATE_CONTINUE;

#ifdef TRACY_ENABLE
    ZoneScoped;
#endif
   
    NOUS_INFO("-------------- PrepareUpdate --------------");

    ret = PrepareUpdate();

    NOUS_INFO("-------------- PreUpdate --------------");

    for (int i = 0; i < NUM_MODULES && ret == UPDATE_CONTINUE; ++i)
    {
        if (list_modules[i] != nullptr) 
        {
            ret = list_modules[i]->PreUpdate(dt);
        }
    }

    NOUS_INFO("-------------- Update --------------");

    for (int i = 0; i < NUM_MODULES && ret == UPDATE_CONTINUE; ++i)
    {
        if (list_modules[i] != nullptr) 
        {
            ret = list_modules[i]->Update(dt);
        }
    }

    NOUS_INFO("-------------- PostUpdate --------------");

    for (int i = 0; i < NUM_MODULES && ret == UPDATE_CONTINUE; ++i)
    {
        if (list_modules[i] != nullptr) 
        {
            ret = list_modules[i]->PostUpdate(dt);
        }
    }

    NOUS_INFO("-------------- FinishUpdate --------------");

    FinishUpdate();

#ifdef TRACY_ENABLE
    FrameMark;
#endif

    return ret;
}

void Application::FinishUpdate()
{
    NOUS_TRACE("%s()", __FUNCTION__);

    // Set Window Title with Debug Info

    static float cachedDt = 0.0f;
    static float cachedFPS = 0.0f;
    static char buffer[256];

    if (updateTitleTimer.ReadMS() >= 100.0f)
    {
        cachedDt = GetDT();
        cachedFPS = GetFPS();

        updateTitleTimer.Start();
    }

    sprintf_s(buffer,
        "%s | dt: %.3f s | FPS: %.2f | Graphics Timer: %.3f s | Frame Count: %d",
        TITLE, cachedDt, cachedFPS, TimeManager::graphicsTimer.ReadSec(), TimeManager::frameCount);

    window->SetTitle(buffer);

    NOUS_DEBUG("-------------- Frame Finished --------------");

    // Adapt according to target FPS

    const float targetFrameTime = 1.0f / targetFPS;
    const float elapsedTime = msTimer.ReadSec();

    if (elapsedTime < targetFrameTime)
    {
        SDL_Delay((targetFrameTime - elapsedTime) * 1000);
    }
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
    targetFPS = FPS;
}

float Application::GetTargetFPS()
{
    return targetFPS;
}

float Application::GetFPS()
{
    return 1 / dt;
}

float Application::GetDT()
{
    return dt;
}

float Application::GetMS()
{
    return dt * 1000;
}