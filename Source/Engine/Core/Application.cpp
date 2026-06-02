#include <Engine/Core/Application.h>
#include "Engine/Modules/ModuleWindow/include/ModuleWindow.h"
#include "Engine/Modules/ModuleInput/include/ModuleInput.h"
#include "Engine/Modules/ModuleCamera3D/include/ModuleCamera3D.h"
#include "Engine/Modules/ModuleResourceManager/include/ModuleResourceManager.h"
#include "Engine/Systems/ResourceManager/Core/ImporterManager/include/ImporterManager.h"
#include "Engine/Systems/ResourceManager/Core/TypeRegistry/include/TypeRegistry.h"
#include "Engine/Modules/ModuleScene/include/ModuleScene.h"
#include "Engine/Modules/ModuleRenderer3D/include/ModuleRenderer3D.h"
#include "Engine/Modules/ModuleAudio/include/ModuleAudio.h"
#include "Engine/Systems/ResourceManager/Types/ResourceAudio/include/ResourceAudio.h"

#include <Engine/Core/MemoryManager/MemoryManager.h>

#include <SDL3/SDL.h>

#include <Engine/Core/TimeManager/TimeManager.h>
#include "Engine/NOUS_Multithreading/NOUS_JobSystem/include/NOUS_JobSystem.h"
#include <Engine/Core/EventSystem/EventSystem.h>
#include "Engine/Core/Logger/Logger.h"
#include "Engine/Scripting/ScriptManager.h"
#include "Engine/NOUS_Multithreading/NOUS_Thread/include/NOUS_Thread.h"

#include <chrono>
#include <cmath>
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

    // 6. SCENE — depends on INPUT (simulation controls), RESOURCE MANAGER (asset
    //    loading), and AUDIO (broker through which CAudioSource reaches the backend,
    //    mirroring how CScript reaches scriptManager).
    listModules.push_back(scene           = NOUS_NEW<ModuleScene>(MemoryTag::APPLICATION,
        eventSystem, jobSystem, input, resourceManager, audio));

    // 7. RENDERER — depends on WINDOW (surface), CAMERA (view/proj), RESOURCE MANAGER (GPU resources), SCENE (render data).
    //    Must be last because RESOURCE MANAGER and SCENE must already exist.
    listModules.push_back(renderer        = NOUS_NEW<ModuleRenderer3D>(MemoryTag::APPLICATION,
        eventSystem, jobSystem, window, camera, resourceManager, scene));

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

    // Call Awake() in all modules
    for (int i = 0; i < static_cast<int>(listModules.size()) && ret; ++i)
    {
        if (listModules[i] != nullptr)
            ret = listModules[i]->Awake();
    }

    // Editor-only setup: runs after all Awakes and before Start().
    if (ret && !m_isGameMode)
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

// ---------------------------------------------------------------------------
// DEBUG — temporary development shortcuts. Remove when editor UI covers these.
// ---------------------------------------------------------------------------
static void HandleDebugKeys(const ModuleInput* input, ModuleScene* scene, nous::engine::multithreading::NOUS_JobSystem* jobSystem,
    ModuleResourceManager* resourceManager, ModuleAudio* audio)
{
    if (input->GetKey(SDL_SCANCODE_Z) == KeyState::DOWN)
        scene->SaveScene(scene->GetCurrentScenePath());

    if (input->GetKey(SDL_SCANCODE_X) == KeyState::DOWN)
        scene->ClearScene();

    if (input->GetKey(SDL_SCANCODE_C) == KeyState::DOWN)
        scene->LoadSceneAsync(scene->GetCurrentScenePath());

    if (input->GetKey(SDL_SCANCODE_F1) == KeyState::DOWN)
        jobSystem->SubmitJob([scene] { scene->SpawnMeshAsHierarchy("Assets/Meshes/Lagiacrus_Head.fbx"); },    "Spawn Lagiacrus");

    if (input->GetKey(SDL_SCANCODE_F2) == KeyState::DOWN)
        jobSystem->SubmitJob([scene] { scene->SpawnMeshAsHierarchy("Assets/Meshes/Cypher_S0_Skelmesh.fbx"); }, "Spawn Cypher");

    if (input->GetKey(SDL_SCANCODE_F3) == KeyState::DOWN)
        jobSystem->SubmitJob([scene] { scene->SpawnMeshAsHierarchy("Assets/Meshes/Queen_Xenomorph.fbx"); },   "Spawn Queen Xenomorph");

    if (input->GetKey(SDL_SCANCODE_F4) == KeyState::DOWN)
        jobSystem->SubmitJob([scene] { scene->SpawnMeshAsHierarchy("Assets/Meshes/Wolf.obj"); },              "Spawn Wolf");

    if (input->GetKey(SDL_SCANCODE_F5) == KeyState::DOWN)
    {
        constexpr auto meshPaths = std::to_array<std::string_view>({
            "Assets/Meshes/Lagiacrus_Head.fbx",
            "Assets/Meshes/Cypher_S0_Skelmesh.fbx",
            "Assets/Meshes/Queen_Xenomorph.fbx",
            "Assets/Meshes/Wolf.obj"
        });

        for (const auto& path : meshPaths)
            jobSystem->SubmitJob([scene, path] { scene->SpawnMeshAsHierarchy(path.data()); }, "Spawn Model");
    }

    if (input->GetKey(SDL_SCANCODE_F6) == KeyState::DOWN)
        scene->ClearScene();

    if (input->GetKey(SDL_SCANCODE_F7) == KeyState::DOWN)
        jobSystem->SubmitJob([] { nous::engine::multithreading::NOUS_Thread::SleepMS(5000); }, "Test Sleep");

    if (input->GetKey(SDL_SCANCODE_F8) == KeyState::DOWN)
    {
        for (int i = 0; i < 100; ++i)
        {
            jobSystem->SubmitJob([]
            {
                constexpr std::chrono::milliseconds duration(500);
                const auto start = std::chrono::steady_clock::now();
                while (std::chrono::steady_clock::now() - start < duration)
                    (void)std::sqrt(123.456);
            }, "Stress Test");
        }
    }

    if (input->GetKey(SDL_SCANCODE_F9) == KeyState::DOWN)
    {
        NOUS_INFO("Initiating script hot-reload...");
        jobSystem->SubmitJob([scene] { scene->RecompileScripts(); }, "Scripts Hot-Reload");
    }

    // F10 — load Assets/Audio/SFX/test.wav through the ResourceManager, log its
    // probed metadata, and play it via ModuleAudio. Validates the full import →
    // deserialize → probe → play path.
    if (input->GetKey(SDL_SCANCODE_F10) == KeyState::DOWN && resourceManager && audio)
    {
        constexpr const char* c_testAudio = "Assets/Audio/SFX/test.wav";
        ResourceBase* res = resourceManager->CreateResource(c_testAudio);
        if (!res || res->GetType() != ResourceType::AUDIO)
        {
            NOUS_WARN("[AudioDebug] Failed to load '%s' through ResourceManager.", c_testAudio);
        }
        else
        {
            auto* rAudio = static_cast<ResourceAudio*>(res);
            NOUS_INFO("[AudioDebug] '%s' UID=%u  fileType=%d  streaming=%d  %.2fs  %uHz  %uch",
                rAudio->GetName().c_str(),
                rAudio->GetUID(),
                static_cast<int>(rAudio->GetFileType()),
                static_cast<int>(rAudio->GetStreamingMode()),
                rAudio->GetDurationSec(),
                rAudio->GetSampleRate(),
                static_cast<uint32>(rAudio->GetChannelCount()));
            audio->PlayAudio(rAudio);
        }

        constexpr const char* c_testMusic = "Assets/Audio/Music/music.ogg";
        ResourceBase* res2 = resourceManager->CreateResource(c_testMusic);
        if (!res2 || res2->GetType() != ResourceType::AUDIO)
        {
            NOUS_WARN("[AudioDebug] Failed to load '%s' through ResourceManager.", c_testMusic);
        }
        else
        {
            auto* rAudio = static_cast<ResourceAudio*>(res2);
            NOUS_INFO("[AudioDebug] '%s' UID=%u  fileType=%d  streaming=%d  %.2fs  %uHz  %uch",
                rAudio->GetName().c_str(),
                rAudio->GetUID(),
                static_cast<int>(rAudio->GetFileType()),
                static_cast<int>(rAudio->GetStreamingMode()),
                rAudio->GetDurationSec(),
                rAudio->GetSampleRate(),
                static_cast<uint32>(rAudio->GetChannelCount()));
            audio->PlayAudio(rAudio);
        }
    }

    // F11 — list every loaded ResourceAudio in the registry with its probe data.
    if (input->GetKey(SDL_SCANCODE_F11) == KeyState::DOWN && resourceManager)
    {
        const auto map = resourceManager->GetResourcesMap();
        NOUS_INFO("[AudioDebug] Loaded audio resources:");
        uint32 audioCount = 0;
        for (const auto& [uid, res] : map)
        {
            if (!res || res->GetType() != ResourceType::AUDIO) continue;
            const auto* rAudio = static_cast<const ResourceAudio*>(res);
            NOUS_INFO("  UID=%u  name='%s'  asset='%s'  lib='%s'  %.2fs  %uHz  %uch  refs=%u  state=%d",
                rAudio->GetUID(),
                rAudio->GetName().c_str(),
                rAudio->GetAssetsPath().c_str(),
                rAudio->GetLibraryPath().c_str(),
                rAudio->GetDurationSec(),
                rAudio->GetSampleRate(),
                static_cast<uint32>(rAudio->GetChannelCount()),
                rAudio->GetReferenceCount(),
                static_cast<int>(rAudio->GetState()));
            ++audioCount;
        }
        NOUS_INFO("[AudioDebug] Total: %u audio resource(s).", audioCount);
    }
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

        // Editor-only authoring shortcuts (spawn debug meshes, clear scene,
        // hot-reload scripts, ...). Disabled in standalone GAME builds.
        if (ret == UpdateStatus::CONTINUE && !m_isGameMode)
            HandleDebugKeys(input, scene, jobSystem, resourceManager, audio);
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
        jobSystem->WaitForPendingJobs();

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

EventSystem*           Application::GetEventSystem()     const { return eventSystem; }

ModuleWindow*          Application::GetWindow()          const { return window; }
ModuleInput*           Application::GetInput()           const { return input; }
ModuleCamera3D*        Application::GetCamera()          const { return camera; }
ModuleResourceManager* Application::GetResourceManager() const { return resourceManager; }
ModuleScene*           Application::GetScene()           const { return scene; }
ModuleRenderer3D*      Application::GetRenderer()        const { return renderer; }
ModuleAudio*           Application::GetAudio()           const { return audio; }

nous::engine::multithreading::NOUS_JobSystem* Application::GetJobSystem() const { return jobSystem; }

