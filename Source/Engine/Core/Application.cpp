#include <Engine/Core/Application.h>
#include "Engine/Modules/ModuleWindow/include/ModuleWindow.h"
#include "Engine/Modules/ModuleInput/include/ModuleInput.h"
#include "Engine/Modules/ModuleFileSystem/include/ModuleFileSystem.h"
#include "Engine/Modules/ModuleCamera3D/include/ModuleCamera3D.h"
#include "Engine/Modules/ModuleResourceManager/include/ModuleResourceManager.h"
#include "Engine/Modules/ModuleScene/include/ModuleScene.h"
#include "Engine/Modules/ModuleRenderer3D/include/ModuleRenderer3D.h"

#include <Engine/Core/MemoryManager/MemoryManager.h>

#include <SDL3/SDL.h>

#include <Engine/Core/TimeManager/TimeManager.h>
#include "Engine/NOUS_Multithreading/NOUS_JobSystem/include/NOUS_JobSystem.h"
#include <Engine/Core/EventSystem/EventSystem.h>
#include <Engine/Core/EventSystem/Event/include/Event.h>
#include "Engine/Core/Logger/Logger.h"

#ifdef _PROFILING
#include <tracy/Tracy.hpp>
#endif

extern Application* External = nullptr;

Application::Application()
{
	External = this;

    isMinimized = false;

    targetFPS = DEFAULT_TARGET_FPS;
    dt = 0.0f;

    eventSystem = NOUS_NEW<EventSystem>(MemoryManager::MemoryTag::APPLICATION);

    msTimer = NOUS_NEW<Timer>(MemoryManager::MemoryTag::APPLICATION);
    updateTitleTimer = NOUS_NEW<Timer>(MemoryManager::MemoryTag::APPLICATION);

    listModules[0] = window = NOUS_NEW<ModuleWindow>(MemoryManager::MemoryTag::APPLICATION, this);
    listModules[1] = input = NOUS_NEW<ModuleInput>(MemoryManager::MemoryTag::APPLICATION, this);
    listModules[2] = fileSystem = NOUS_NEW<ModuleFileSystem>(MemoryManager::MemoryTag::APPLICATION, this);
    listModules[3] = camera = NOUS_NEW<ModuleCamera3D>(MemoryManager::MemoryTag::APPLICATION, this);
    listModules[4] = resourceManager = NOUS_NEW<ModuleResourceManager>(MemoryManager::MemoryTag::APPLICATION, this);
    listModules[5] = scene = NOUS_NEW<ModuleScene>(MemoryManager::MemoryTag::APPLICATION, this);
    listModules[6] = renderer = NOUS_NEW<ModuleRenderer3D>(MemoryManager::MemoryTag::APPLICATION, this);

    // ------------- MULTITHREADING ------------- //
    jobSystem = NOUS_NEW<NOUS_Multithreading::NOUS_JobSystem>(MemoryManager::MemoryTag::THREAD);
}

Application::~Application()
{
    if (jobSystem)
        jobSystem->WaitForPendingJobs();

    for (auto* mod : listModules)
        NOUS_DELETE(mod, MemoryManager::MemoryTag::APPLICATION);

    NOUS_DELETE(jobSystem, MemoryManager::MemoryTag::THREAD);
    NOUS_DELETE(eventSystem, MemoryManager::MemoryTag::APPLICATION);
    NOUS_DELETE(msTimer, MemoryManager::MemoryTag::APPLICATION);
    NOUS_DELETE(updateTitleTimer, MemoryManager::MemoryTag::APPLICATION);
}

bool Application::Awake()
{
    bool ret = true;

    // Call Awake() in all modules
    for (int i = 0; i < NUM_MODULES && ret; ++i)
    {
        if (listModules[i] != nullptr) 
        {
            ret = listModules[i]->Awake();
        }
    }

    return ret;
}

bool Application::Start()
{
    bool ret = true;

    // After all Awake calls we call Start() in all modules
    NOUS_INFO("-------------- Application Start --------------");
    for (int i = 0; i < NUM_MODULES && ret; ++i)
    {
        if (listModules[i] != nullptr)
        {
            ret = listModules[i]->Start();
        }
    }

    NOUS_INFO(MemoryManager::GetMemoryUsageStats().c_str());

    msTimer->Start();

    TimeManager::frameCount = 0;
    TimeManager::graphicsTimer.Start();

    updateTitleTimer->Start();

    return ret;
}

UpdateStatus Application::PrepareUpdate()
{
    NOUS_TRACE("%s()", __FUNCTION__);

    dt = msTimer->ReadSec();
    msTimer->Start();

    TimeManager::deltaTime = dt;
    TimeManager::frameCount++;

    return UpdateStatus::CONTINUE;
}

UpdateStatus Application::Update()
{
    UpdateStatus ret = UpdateStatus::CONTINUE;

#ifdef _PROFILING
    ZoneScoped;
#endif
   
    NOUS_TRACE("-------------- PrepareUpdate --------------");

    ret = PrepareUpdate();

    NOUS_TRACE("-------------- PreUpdate --------------");

    for (int i = 0; i < NUM_MODULES && ret == UpdateStatus::CONTINUE; ++i)
    {
        if (listModules[i] != nullptr) 
        {
            ret = listModules[i]->PreUpdate(dt);
        }
    }

    NOUS_TRACE("-------------- Update --------------");

    for (int i = 0; i < NUM_MODULES && ret == UpdateStatus::CONTINUE; ++i)
    {
        if (listModules[i] != nullptr) 
        {
            ret = listModules[i]->Update(dt);
        }
    }

    NOUS_TRACE("-------------- PostUpdate --------------");

    for (int i = 0; i < NUM_MODULES && ret == UpdateStatus::CONTINUE; ++i)
    {
        if (listModules[i] != nullptr) 
        {
            ret = listModules[i]->PostUpdate(dt);
        }
    }

    NOUS_TRACE("-------------- FinishUpdate --------------");

    FinishUpdate();

#ifdef _PROFILING
    FrameMark;
#endif

    return ret;
}

void Application::FinishUpdate()
{
    NOUS_TRACE("%s()", __FUNCTION__);

    eventSystem->DispatchQueued();

    // Set Window Title with Debug Info

    static float cachedDt = 0.0f;
    static float cachedFPS = 0.0f;
    static char buffer[256];

    if (updateTitleTimer->ReadMS() >= 100.0f)
    {
        cachedDt = GetDT();
        cachedFPS = GetFPS();

        updateTitleTimer->Start();
    }

    sprintf(buffer,
        "%s | dt: %.3f s | FPS: %.2f | Graphics Timer: %.3f s | Frame Count: %d",
        TITLE, cachedDt, cachedFPS, TimeManager::graphicsTimer.ReadSec(), TimeManager::frameCount);

    window->SetTitle(buffer);

    NOUS_TRACE("-------------- Frame Finished --------------");

    // Adapt according to target FPS

    const float targetFrameTime = 1.0f / targetFPS;
    const float elapsedTime = msTimer->ReadSec();

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
        if (listModules[i] != nullptr) {
            ret = listModules[i]->CleanUp();
        }
    }
    return ret;
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

void Application::QueueEvent(const Event &event)
{
    eventSystem->Queue(event);
}

void Application::BroadcastEvent(const Event &event)
{
    eventSystem->Broadcast(event);
}
