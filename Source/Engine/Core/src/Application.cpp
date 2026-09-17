#include <Core/Application.h>
#include <EngineCore/AppConfig.h>

#include <ResourceManager/Core/ImporterManager.h>
#include <ResourceManager/Core/TypeRegistry.h>

#include <ModuleWindow/ModuleWindow.h>
#include <ModuleInput/ModuleInput.h>
#include <ModuleCamera3D/ModuleCamera3D.h>
#include <ModuleResourceManager/ModuleResourceManager.h>
#include <ModuleAudio/ModuleAudio.h>
#include <ModuleVideo/ModuleVideo.h>
#include <ModuleUI/ModuleUI.h>
#include <ModuleAI/ModuleAI.h>
#include <ModulePhysics/ModulePhysics.h>
#include <ModuleParticles/ModuleParticles.h>
#include <ModuleScene/ModuleScene.h>
#include <ModuleRenderer3D/ModuleRenderer3D.h>

#include <MemoryManager/MemoryManager.h>

#include <SDL3/SDL.h>

#include <TimeManager/TimeManager.h>
#include <NOUS_Multithreading/NOUS_JobSystem.h>
#include <EventSystem/EventSystem.h>
#include <Logger/Logger.h>
#include <Scripting/ScriptManager.h>
#include <ECS/ComponentServices.h>

#include <vector>
#include <string>

#ifdef _PROFILING
#include <tracy/Tracy.hpp>
#endif

Application::Application(const bool isGameMode)
{
    m_isGameMode = isGameMode;

    targetFPS = DEFAULT_TARGET_FPS;
    dt = 0.0f;

    eventSystem       = NOUS_NEW<EventSystem>(MemoryTag::APPLICATION);

    // Resource type registry — must exist before any module is constructed,
    // since modules and importers will pull descriptors from it.
    typeRegistry = NOUS_NEW<TypeRegistry>(MemoryTag::APPLICATION);
    RegisterResourceTypes(*typeRegistry);

    importerManager   = NOUS_NEW<ImporterManager>(MemoryTag::APPLICATION, *typeRegistry);

    msTimer = NOUS_NEW<Timer>(MemoryTag::APPLICATION);
    updateTitleTimer = NOUS_NEW<Timer>(MemoryTag::APPLICATION);

    // ------------- MULTITHREADING ------------- //
    jobSystem = NOUS_NEW<nous::engine::multithreading::NOUS_JobSystem>(MemoryTag::THREAD);

    // -----------------------------------------------------------------------
    // Module construction order — LOAD-BEARING. Do not reorder.
    //
    // Each module receives its dependencies at construction time. A module
    // must be constructed AFTER every module it depends on. The insertion
    // order also defines the Awake/Start/Update call order and the REVERSE
    // CleanUp/destruction order, so correctness depends on this sequence.
    //
    // Dependency graph:
    //   WINDOW
    //     └─ INPUT
    //          └─ CAMERA
    //   RESOURCE MANAGER
    //     └─ SCENE (also depends on INPUT)
    //   RENDERER (depends on WINDOW, CAMERA, RESOURCE MANAGER, SCENE)
    //   EDITOR   (depends on all of the above — constructed in MainEditor.cpp)
    // -----------------------------------------------------------------------

    // 1. WINDOW — no module dependencies.
    listModules.push_back(window          = NOUS_NEW<ModuleWindow>(MemoryTag::APPLICATION,
        eventSystem, jobSystem));

    // 2. INPUT — no module dependencies.
    listModules.push_back(input           = NOUS_NEW<ModuleInput>(MemoryTag::APPLICATION,
        eventSystem, jobSystem));

    // 3. CAMERA — depends on INPUT (reads input state for editor camera movement).
    listModules.push_back(camera          = NOUS_NEW<ModuleCamera3D>(MemoryTag::APPLICATION,
        eventSystem, jobSystem, input));

    // 4. RESOURCE MANAGER — no module dependencies at construction.
    //    Must be constructed before SCENE and RENDERER so they can reference it.
    listModules.push_back(resourceManager = NOUS_NEW<ModuleResourceManager>(MemoryTag::APPLICATION,
        eventSystem, jobSystem, importerManager, *typeRegistry));

    // 5. AUDIO — no module dependencies. Constructed BEFORE SCENE so it is torn
    //    down AFTER it (modules clean up in reverse): CAudioSource::OnDestroy
    //    releases its voice back into the audio backend during scene teardown,
    //    exactly as CMesh::OnDestroy relies on RESOURCE MANAGER still being alive.
    listModules.push_back(audio           = NOUS_NEW<ModuleAudio>(MemoryTag::APPLICATION,
        eventSystem, jobSystem));

    // VIDEO — like AUDIO, no module dependencies and constructed BEFORE SCENE so it tears
    // down AFTER it (modules clean up in reverse). The Phase-3 CVideoPlayer::OnDestroy
    // releases its decoder handle through ModuleVideo during scene teardown, exactly as
    // CAudioSource relies on ModuleAudio still being alive.
    listModules.push_back(video           = NOUS_NEW<ModuleVideo>(MemoryTag::APPLICATION,
        eventSystem, jobSystem));

    // PLACEHOLDERS — skeleton modules, no behaviour and no module dependencies yet.
    // Constructed BEFORE SCENE for the same reason AUDIO and VIDEO are: whatever
    // component eventually reaches into them during scene teardown needs them alive.
    listModules.push_back(ui              = NOUS_NEW<ModuleUI>(MemoryTag::APPLICATION,
        eventSystem, jobSystem));

    listModules.push_back(ai              = NOUS_NEW<ModuleAI>(MemoryTag::APPLICATION,
        eventSystem, jobSystem));

    listModules.push_back(physics         = NOUS_NEW<ModulePhysics>(MemoryTag::APPLICATION,
        eventSystem, jobSystem));

    listModules.push_back(particles       = NOUS_NEW<ModuleParticles>(MemoryTag::APPLICATION,
        eventSystem, jobSystem));

    // 6. SCENE — depends on INPUT (simulation controls), RESOURCE MANAGER (asset
    //    loading), and AUDIO (broker through which CAudioSource reaches the backend,
    //    mirroring how CScript reaches scriptManager).
    listModules.push_back(scene           = NOUS_NEW<ModuleScene>(MemoryTag::APPLICATION,
        eventSystem, jobSystem, input, resourceManager, audio, video));

    // 7. RENDERER — depends on WINDOW (surface), CAMERA (view/proj), RESOURCE MANAGER (GPU resources), SCENE (render data).
    //    Must be last because RESOURCE MANAGER and SCENE must already exist.
    //    The resource manager goes in three times on purpose: ModuleRenderer3D
    //    takes IResourceGpuSync + IResourceLoader + IRenderResourceProvider, not
    //    the module, so the upcast happens here (the composition root already
    //    knows the concrete type) instead of inside the render module.
    listModules.push_back(renderer        = NOUS_NEW<ModuleRenderer3D>(MemoryTag::APPLICATION,
        eventSystem, jobSystem, window, camera,
        resourceManager, resourceManager, resourceManager,   // narrowed to 3 interfaces
        scene));

    // Assemble the component service seam — what lets Systems/ reach engine services
    // without depending on Modules/. See ComponentServices.h. Each module upcasts to
    // the interface it implements. Runs here, after the whole graph exists; ModuleScene
    // has already built its Scene, which is fine because Scene holds a pointer to
    // ModuleScene::m_componentServices rather than a copy of it.
    ComponentServices services;
    services.host      = scene;
    services.audio     = audio;
    services.video     = video;
    services.resources = resourceManager;
    services.scripts   = scene->GetScriptRegistry();  // created in ModuleScene's ctor, non-null here
    scene->SetComponentServices(services);

    if (m_isGameMode)
    {
        renderer->SetRenderMode(RenderMode::GAME);
        resourceManager->SetGameMode();
        input->SetGameMode(true);
    }

    // 8. EDITOR — depends on all modules above.
    //    Constructed externally in MainEditor.cpp after this constructor returns.
}

Application::~Application()
{
    // CleanUp() already drained jobs; this is a safety net for abnormal exits.
    if (jobSystem)
        jobSystem->WaitForPendingJobs();

    // Delete modules in REVERSE registration order, matching the CleanUp order.
    // Forward order would free ModuleResourceManager before ModuleScene,
    // causing use-after-free when ~ModuleScene destroys GameObjects whose
    // CMesh::OnDestroy still references the ResourceManager.
    for (int i = static_cast<int>(listModules.size()) - 1; i >= 0; --i)
        NOUS_DELETE(listModules[i], MemoryTag::APPLICATION);

    NOUS_DELETE(jobSystem, MemoryTag::THREAD);
    NOUS_DELETE(eventSystem, MemoryTag::APPLICATION);
    NOUS_DELETE(importerManager, MemoryTag::APPLICATION);
    // Registry is destroyed last among engine-owned singletons because importers
    // (still referenced from importerManager above) may transitively touch it.
    NOUS_DELETE(typeRegistry, MemoryTag::APPLICATION);
    NOUS_DELETE(msTimer, MemoryTag::APPLICATION);
    NOUS_DELETE(updateTitleTimer, MemoryTag::APPLICATION);
}

bool Application::Awake() const
{
    bool ret = true;
    int failedIndex = -1;

    // Call Awake() in all modules
    for (int i = 0; i < static_cast<int>(listModules.size()) && ret; ++i)
    {
        if (listModules[i] != nullptr)
        {
            ret = listModules[i]->Awake();
            if (!ret)
                failedIndex = i;
        }
    }

    // A module's Awake() failed. The main loop bails straight to exit on a false
    // return (skipping CleanUp()), and ~Application only deletes modules — it does
    // not CleanUp() them. So the modules that already awoke would leak the resources
    // they acquired (e.g. the audio/video backends, freed in CleanUp(), not in their
    // destructors). Tear them down here in reverse order. The failed module is
    // expected to have cleaned up after itself within its own Awake().
    if (!ret)
    {
        NOUS_ERROR("Module Awake() failed at index %d — cleaning up already-awoken modules.", failedIndex);

        // Mirror CleanUp(): drain in-flight jobs before any module tears down.
        if (jobSystem)
            jobSystem->WaitForPendingJobs();

        for (int i = failedIndex - 1; i >= 0; --i)
        {
            if (listModules[i] != nullptr)
                listModules[i]->CleanUp();
        }

        return false;
    }

    // Editor-only setup: runs after all Awakes and before Start().
    if (!m_isGameMode)
    {
        window->Maximize();
        resourceManager->ScanAndImportAssets();
        scene->SetSnapshotEnabled(true);
    }

    return ret;
}

bool Application::Start() const
{
    bool ret = true;

    // After all Awake calls we call Start() in all modules
    NOUS_INFO("-------------- Application Start --------------");
    for (int i = 0; i < static_cast<int>(listModules.size()) && ret; ++i)
    {
        if (listModules[i] != nullptr)
        {
            ret = listModules[i]->Start();
        }
    }

    LogOutputMultiline(LOG_LEVEL_INFO, LogChannel::NOUS_ENGINE_CORE_APPLICATION,
        (std::string("[Application::Start] ") + nous::engine::memory::GetMemoryUsageStats()).c_str());

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
    auto ret = UpdateStatus::CONTINUE;

#ifdef _PROFILING
    ZoneScoped;
#endif

    // -------------- PrepareUpdate --------------

    ret = PrepareUpdate();

    // -------------- PreUpdate --------------

    {
#ifdef _PROFILING
        ZoneScopedN("PreUpdate");
#endif
        // Apply work workers queued for the main thread (entity creation, scene
        // deserialisation) BEFORE any module reads the registry this frame.
        jobSystem->DrainMainThreadQueue();

        for (int i = 0; i < static_cast<int>(listModules.size()) && ret == UpdateStatus::CONTINUE; ++i)
        {
            if (listModules[i] != nullptr)
                ret = listModules[i]->PreUpdate(dt);
        }
    }

    // -------------- Update --------------

    {
#ifdef _PROFILING
        ZoneScopedN("Update");
#endif
        for (int i = 0; i < static_cast<int>(listModules.size()) && ret == UpdateStatus::CONTINUE; ++i)
        {
            if (listModules[i] != nullptr)
                ret = listModules[i]->Update(dt);
        }

    }

    // -------------- PostUpdate --------------

    {
#ifdef _PROFILING
        ZoneScopedN("PostUpdate");
#endif
        for (int i = 0; i < static_cast<int>(listModules.size()) && ret == UpdateStatus::CONTINUE; ++i)
        {
            if (listModules[i] != nullptr)
                ret = listModules[i]->PostUpdate(dt);
        }
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
#ifdef _PROFILING
    ZoneScopedN("FinishUpdate");
#endif

    {
#ifdef _PROFILING
        ZoneScopedN("DispatchQueuedEvents");
#endif
        eventSystem->DispatchQueued();
    }

    // Set Window Title with Debug Info

    if (updateTitleTimer->ReadMS() >= 100.0f)
    {
        cachedDt  = GetDT();
        cachedFPS = GetFPS();

        updateTitleTimer->Start();
    }

    snprintf(titleBuffer, sizeof(titleBuffer),
        "%s | dt: %.3f s | FPS: %d | Graphics Timer: %.3f s | Frame Count: %d",
        TITLE, cachedDt, static_cast<int>(cachedFPS + 0.5f), TimeManager::graphicsTimer.ReadSec(), TimeManager::frameCount);

    window->SetTitle(titleBuffer);

    // -------------- Frame Finished --------------

    // Frame pacing: target a precise frame time.
    // SDL_Delay has ~1ms granularity on Windows — too coarse for 144Hz (~6.94ms/frame).
    // Strategy: SDL_Delay for the bulk of the remaining time (minus a 2ms safety buffer),
    // then spin-wait for sub-millisecond precision on the final stretch.

    const float targetFrameTime = 1.0f / targetFPS;

    if (const float remaining = targetFrameTime - msTimer->ReadSec(); remaining > 0.0f)
    {
#ifdef _PROFILING
        ZoneScopedN("FramePacing");
#endif
        if (remaining > DEFAULT_SPIN_THRESHOLD)
        {
            SDL_Delay(static_cast<Uint32>((remaining - DEFAULT_SPIN_THRESHOLD) * 1000.0f));
        }

        // Spin-wait for the final portion — burns CPU briefly but gives precise timing.
        while (true)
        {
            if (msTimer->ReadSec() >= targetFrameTime)
                break;
        }
    }
}

bool Application::CleanUp() const
{
    bool ret = true;

    // Drain all in-flight jobs before any module starts tearing down.
    // This ensures all in-flight job lambdas complete while everything is still alive.
    if (jobSystem)
    {
        jobSystem->WaitForPendingJobs();

        // Stop accepting deferred main-thread work; anything still queued is dropped.
        // Running entity construction while modules tear down is worse than losing a
        // spawn the user will never see.
        jobSystem->CloseMainThreadQueue();
    }

    for (int i = static_cast<int>(listModules.size()) - 1; i >= 0 && ret; --i)
    {
        if (listModules[i] != nullptr) {
            ret = listModules[i]->CleanUp();
        }
    }
    return ret;
}

bool Application::IsGameMode() const
{
    return m_isGameMode;
}

void Application::SetTargetFPS(const float FPS)
{
    targetFPS = FPS;
}

float Application::GetTargetFPS() const
{
    return targetFPS;
}

float Application::GetFPS() const
{
    return dt > 0.0001f ? 1.0f / dt : 0.0f;
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

EventSystem*            Application::GetEventSystem()       const { return eventSystem; }

ModuleWindow*           Application::GetWindow()            const { return window; }
ModuleInput*            Application::GetInput()             const { return input; }
ModuleCamera3D*         Application::GetCamera()            const { return camera; }
ModuleResourceManager*  Application::GetResourceManager()   const { return resourceManager; }
ModuleAudio*            Application::GetAudio()             const { return audio; }
ModuleVideo*            Application::GetVideo()             const { return video; }
ModuleUI*			    Application::GetUI()                const { return ui; }
ModuleAI*			    Application::GetAI()                const { return ai; }
ModulePhysics*		    Application::GetPhysics()           const { return physics; }
ModuleParticles*	    Application::GetParticles()         const { return particles; }
ModuleScene*            Application::GetScene()             const { return scene; }
ModuleRenderer3D*       Application::GetRenderer()          const { return renderer; }


nous::engine::multithreading::NOUS_JobSystem* Application::GetJobSystem() const { return jobSystem; }

