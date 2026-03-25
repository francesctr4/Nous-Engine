#include <Engine/Core/Application.h>
#include "Engine/Modules/ModuleWindow/include/ModuleWindow.h"
#include "Engine/Modules/ModuleInput/include/ModuleInput.h"
#include "Engine/Modules/ModuleCamera3D/include/ModuleCamera3D.h"
#include "Engine/Modules/ModuleResourceManager/include/ModuleResourceManager.h"
#include "Engine/Modules/ModuleScene/include/ModuleScene.h"
#include "Engine/Modules/ModuleRenderer3D/include/ModuleRenderer3D.h"

#include <Engine/Core/MemoryManager/MemoryManager.h>

#include <SDL3/SDL.h>

#include <Engine/Core/TimeManager/TimeManager.h>
#include "Engine/NOUS_Multithreading/NOUS_JobSystem/include/NOUS_JobSystem.h"
#include <Engine/Core/EventSystem/EventSystem.h>
#include "Engine/Core/Logger/Logger.h"

#ifdef _PROFILING
#include <tracy/Tracy.hpp>
#endif

NOUS_ENGINE_API Application* External = nullptr;

Application::Application()
{
	External = this;

    isMinimized = false;
    isGameMode  = false;

    targetFPS = DEFAULT_TARGET_FPS;
    dt = 0.0f;

    eventSystem = NOUS_NEW<EventSystem>(MemoryTag::APPLICATION);

    msTimer = NOUS_NEW<Timer>(MemoryTag::APPLICATION);
    updateTitleTimer = NOUS_NEW<Timer>(MemoryTag::APPLICATION);

    listModules[0] = window = NOUS_NEW<ModuleWindow>(MemoryTag::APPLICATION, this);
    listModules[1] = input = NOUS_NEW<ModuleInput>(MemoryTag::APPLICATION, this);
    listModules[2] = camera = NOUS_NEW<ModuleCamera3D>(MemoryTag::APPLICATION, this);
    listModules[3] = resourceManager = NOUS_NEW<ModuleResourceManager>(MemoryTag::APPLICATION, this);
    listModules[4] = scene = NOUS_NEW<ModuleScene>(MemoryTag::APPLICATION, this);
    listModules[5] = renderer = NOUS_NEW<ModuleRenderer3D>(MemoryTag::APPLICATION, this);

    // ------------- MULTITHREADING ------------- //
    jobSystem = NOUS_NEW<NOUS_Multithreading::NOUS_JobSystem>(MemoryTag::THREAD);
}

Application::~Application()
{
    if (jobSystem)
        jobSystem->WaitForPendingJobs();

    // Delete modules in REVERSE order (5→0), matching the CleanUp order.
    // Forward order would free ModuleResourceManager (3) before ModuleScene (4),
    // causing use-after-free when ~ModuleScene destroys GameObjects whose
    // CMesh::OnDestroy still references the ResourceManager.
    for (int i = NUM_MODULES - 1; i >= 0; --i)
        NOUS_DELETE(listModules[i], MemoryTag::APPLICATION);

    NOUS_DELETE(jobSystem, MemoryTag::THREAD);
    NOUS_DELETE(eventSystem, MemoryTag::APPLICATION);
    NOUS_DELETE(msTimer, MemoryTag::APPLICATION);
    NOUS_DELETE(updateTitleTimer, MemoryTag::APPLICATION);
}

bool Application::Awake() const
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

bool Application::Start() const
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

    LogOutputMultiline(LOG_LEVEL_INFO, LogChannel::NOUS_ENGINE_CORE_APPLICATION,
        (std::string("[Application::Start] ") + MemoryManager::GetMemoryUsageStats()).c_str());

    msTimer->Start();

    TimeManager::frameCount = 0;
    TimeManager::graphicsTimer.Start();

    updateTitleTimer->Start();

    return ret;
}

UpdateStatus Application::PrepareUpdate()
{
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

    // -------------- PrepareUpdate --------------

    ret = PrepareUpdate();

    // -------------- PreUpdate --------------

    for (int i = 0; i < NUM_MODULES && ret == UpdateStatus::CONTINUE; ++i)
    {
        if (listModules[i] != nullptr)
            ret = listModules[i]->PreUpdate(dt);
    }

    // -------------- Update --------------

    for (int i = 0; i < NUM_MODULES && ret == UpdateStatus::CONTINUE; ++i)
    {
        if (listModules[i] != nullptr)
            ret = listModules[i]->Update(dt);
    }

    // -------------- PostUpdate --------------

    for (int i = 0; i < NUM_MODULES && ret == UpdateStatus::CONTINUE; ++i)
    {
        if (listModules[i] != nullptr)
            ret = listModules[i]->PostUpdate(dt);
    }

    // -------------- FinishUpdate --------------

    FinishUpdate();

#ifdef _PROFILING
    FrameMark;
#endif

    return ret;
}

void Application::FinishUpdate() const
{
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

    snprintf(buffer, sizeof(buffer),
        "%s | dt: %.3f s | FPS: %d | Graphics Timer: %.3f s | Frame Count: %d",
        TITLE, cachedDt, static_cast<int>(cachedFPS + 0.5f), TimeManager::graphicsTimer.ReadSec(), TimeManager::frameCount);

    window->SetTitle(buffer);

    // -------------- Frame Finished --------------

    // Frame pacing: target a precise frame time.
    // SDL_Delay has ~1ms granularity on Windows — too coarse for 144Hz (~6.94ms/frame).
    // Strategy: SDL_Delay for the bulk of the remaining time (minus a 2ms safety buffer),
    // then spin-wait for sub-millisecond precision on the final stretch.

    const float targetFrameTime = 1.0f / targetFPS;
    const float remaining = targetFrameTime - msTimer->ReadSec();

    if (remaining > 0.0f)
    {
        constexpr float spinThreshold = 0.002f; // spin for the last 2ms

        if (remaining > spinThreshold)
        {
            SDL_Delay(static_cast<Uint32>((remaining - spinThreshold) * 1000.0f));
        }

        // Spin-wait for the final portion — burns CPU briefly but gives precise timing.
        while (msTimer->ReadSec() < targetFrameTime) {}
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

float Application::GetTargetFPS() const
{
    return targetFPS;
}

float Application::GetFPS() const
{
    return 1 / dt;
}

float Application::GetDT() const
{
    return dt;
}

float Application::GetMS() const
{
    return dt * 1000;
}

void Application::QueueEvent(const Event &event) const
{
    eventSystem->Queue(event);
}

void Application::BroadcastEvent(const Event &event) const
{
    eventSystem->Broadcast(event);
}
