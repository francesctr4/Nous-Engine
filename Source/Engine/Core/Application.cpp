#include "Application.h"
#include "Engine/Core/Modules/ModuleWindow.h"
#include "Engine/Core/Modules/ModuleInput.h"
#include "Engine/Core/Modules/ModuleFileSystem.h"
#include "Engine/Core/Modules/ModuleCamera3D.h"
#include "Engine/Core/Modules/ModuleResourceManager.h"
#include "Engine/Core/Modules/ModuleScene.h"
#include "Engine/Core/Modules/ModuleRenderer3D.h"
#include "Engine/Core/Modules/ModuleEditor.h"

#include "Engine/Utils/Logger.h"
#include "Engine/Systems/Memory Manager/MemoryManager.h"

#include "SDL3/SDL.h"

#include "Engine/Systems/Time Management/TimeManager.h"
#include "Engine/Multithreading/NOUS_JobSystem.h"

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

    msTimer = NOUS_NEW<Timer>(MemoryManager::MemoryTag::APPLICATION);
    updateTitleTimer = NOUS_NEW<Timer>(MemoryManager::MemoryTag::APPLICATION);

    listModules[0] = window = NOUS_NEW<ModuleWindow>(MemoryManager::MemoryTag::APPLICATION, this);
    listModules[1] = input = NOUS_NEW<ModuleInput>(MemoryManager::MemoryTag::APPLICATION, this);
    listModules[2] = fileSystem = NOUS_NEW<ModuleFileSystem>(MemoryManager::MemoryTag::APPLICATION, this);
    listModules[3] = camera = NOUS_NEW<ModuleCamera3D>(MemoryManager::MemoryTag::APPLICATION, this);
    listModules[4] = resourceManager = NOUS_NEW<ModuleResourceManager>(MemoryManager::MemoryTag::APPLICATION, this);
    listModules[5] = scene = NOUS_NEW<ModuleScene>(MemoryManager::MemoryTag::APPLICATION, this);
    listModules[6] = renderer = NOUS_NEW<ModuleRenderer3D>(MemoryManager::MemoryTag::APPLICATION, this);
    listModules[7] = editor = NOUS_NEW<ModuleEditor>(MemoryManager::MemoryTag::APPLICATION, this);

    // ------------- MULTITHREADING ------------- //
    jobSystem = NOUS_NEW<NOUS_Multithreading::NOUS_JobSystem>(MemoryManager::MemoryTag::THREAD);
}

Application::~Application()
{
    NOUS_DELETE(msTimer, MemoryManager::MemoryTag::APPLICATION);
    NOUS_DELETE(updateTitleTimer, MemoryManager::MemoryTag::APPLICATION);

    // ------------- MULTITHREADING ------------- //
    jobSystem->WaitForPendingJobs();
    NOUS_DELETE<NOUS_Multithreading::NOUS_JobSystem>(jobSystem, MemoryManager::MemoryTag::THREAD);

    ModuleEditor* editor = static_cast<ModuleEditor*>(listModules[7]);
    NOUS_DELETE<ModuleEditor>(editor, MemoryManager::MemoryTag::APPLICATION);

    ModuleRenderer3D* renderer = static_cast<ModuleRenderer3D*>(listModules[6]);
    NOUS_DELETE<ModuleRenderer3D>(renderer, MemoryManager::MemoryTag::APPLICATION);

    ModuleScene* scene = static_cast<ModuleScene*>(listModules[5]);
    NOUS_DELETE<ModuleScene>(scene, MemoryManager::MemoryTag::APPLICATION);

    ModuleResourceManager* resourceManager = static_cast<ModuleResourceManager*>(listModules[4]);
    NOUS_DELETE<ModuleResourceManager>(resourceManager, MemoryManager::MemoryTag::APPLICATION);

    ModuleCamera3D* camera = static_cast<ModuleCamera3D*>(listModules[3]);
    NOUS_DELETE<ModuleCamera3D>(camera, MemoryManager::MemoryTag::APPLICATION);

    ModuleFileSystem* fileSystem = static_cast<ModuleFileSystem*>(listModules[2]);
    NOUS_DELETE<ModuleFileSystem>(fileSystem, MemoryManager::MemoryTag::APPLICATION);

    ModuleInput* input = static_cast<ModuleInput*>(listModules[1]);
    NOUS_DELETE<ModuleInput>(input, MemoryManager::MemoryTag::APPLICATION);

    ModuleWindow* window = static_cast<ModuleWindow*>(listModules[0]);
    NOUS_DELETE<ModuleWindow>(window, MemoryManager::MemoryTag::APPLICATION);
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

    // After all Awake calls we call Start() in all modules
    NOUS_INFO("-------------- Application Start --------------");
    for (int i = 0; i < NUM_MODULES && ret; ++i)
    {
        if (listModules[i] != nullptr) 
        {
            ret = listModules[i]->Start();
        }
    }

    NOUS_INFO(MemoryManager::GetMemoryUsageStats());

    msTimer->Start();

    TimeManager::frameCount = 0;
    TimeManager::graphicsTimer.Start();

    updateTitleTimer->Start();

    NOUS_DEBUG(" -------------- ENGINE START UP TIME: %.3f seconds --------------\n", startupTimer.ReadSec());

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

void Application::BroadcastEvent(const Event& event)
{
    for (int i = 0; i < NUM_MODULES; ++i)
    {
        if (listModules[i] != nullptr) 
        {
            listModules[i]->ReceiveEvent(event);
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