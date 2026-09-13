#include <EditorUI/MainMenuBar.h>
#include <EditorUI/AssetsBrowser.h>   // the Assets menu drives the browser's state
#include <ModuleEditor/GameExporter.h>
#include <ModuleResourceManager/ModuleResourceManager.h>

#include "imgui.h"
#include <ModuleScene/ModuleScene.h>
#include <ECS/Scene/Scene.h>
#include <ECS/GameObject.h>
#include <ECS/Component/Types/CCamera/CCamera.h>
#include <ECS/Component/Types/CLight/CLight.h>
#include <RendererFrontend/RendererFrontend.h>
#include <FileSystem/FileSystem.h>
#include <Logger/Logger.h>
#include <NOUS_Multithreading/NOUS_JobSystem.h>   // WaitForPendingJobs before clearing
#include <ResourceManager/Runtime/ImportPipeline.h>

#include <SDL3/SDL.h>

#include <cstdlib>    // std::system -- the CopyAssets target
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#include <shellapi.h>
#include <shlobj.h>
#elif defined(__APPLE__)
#include <cstdlib>
#include <unistd.h>
#elif defined(__linux__)
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>
#endif

static std::string AssetPathToLibraryScene(const std::string& assetPath)
{
    return "Library/Scenes/" + std::filesystem::path(assetPath).filename().string();
}

static std::string GetDefaultBuildPath()
{
    const char* base = SDL_GetBasePath();
    std::filesystem::path p = base ? base : ".";
    std::string s = p.string();
    if (!s.empty() && (s.back() == '/' || s.back() == '\\'))
        s.pop_back();
    return (std::filesystem::path(s) / "Delivery" / "Game").string();
}

static bool RequestBrowser(const char* url)
{
#ifdef _WIN32
    HINSTANCE result = ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOWNORMAL);
    return (INT_PTR)result > 32; // ShellExecute devuelve > 32 si es exitoso
#elif defined(__APPLE__)
    pid_t pid = fork();
    if (pid == 0)
    {
        execl("/usr/bin/open", "open", url, nullptr);
        exit(1);
    }
    return pid > 0;
#elif defined(__linux__)
    pid_t pid = fork();
    if (pid == 0)
    {
        execl("/usr/bin/xdg-open", "xdg-open", url, nullptr);
        exit(1);
    }
    return pid > 0;
#else
    return false; // Plataforma no soportada
#endif
}

MainMenuBar::MainMenuBar(const char* title, EditorContext* context, bool start_open)
    : IEditorWindow(title, context, nullptr, start_open)
{
}

void MainMenuBar::saveToCurrent(bool& triggerSaveAs) const
{
    ModuleScene* scene = editorContext->GetScene();
    if (scene->HasCurrentScenePath())
        scene->SaveScene(scene->GetCurrentScenePath());
    else
        triggerSaveAs = true;
}

bool MainMenuBar::UpdatesWhenCollapsed() const
{
    return true;
}

void MainMenuBar::Update()
{
    openSaveAs     = false;
    openOpen       = false;
    openNewScene   = false;
    openClearScene = false;

    m_openBuild         = false;
    m_openBuildAndRun   = false;
    m_openBuildSettings = false;

    // Keyboard shortcuts (only when no modal dialog is open).
    if (!ImGui::IsPopupOpen(kSaveAsPopup) && !ImGui::IsPopupOpen(kOpenPopup)
        && !ImGui::IsPopupOpen(kNewScenePopup) && !ImGui::IsPopupOpen(kClearScenePopup))
    {
        const ImGuiIO& io = ImGui::GetIO();
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
        {
            if (io.KeyShift || io.KeyAlt)
                openSaveAs = true;
            else
                saveToCurrent(openSaveAs);
        }
    }
}

void MainMenuBar::DrawContent()
{
    if (ImGui::BeginMenu("File"))
    {
        ImGui::SeparatorText("Scene");

        if (ImGui::MenuItem("New Scene"))
        {
            openNewScene = true;
        }

        if (ImGui::MenuItem("Open Scene"))
        {
            openOpen = true;
        }

        // Discards unsaved changes by reloading from disk -- the one scene action
        // with no other route, and what the old debug C key did. Disabled for an
        // unsaved scene, which has no file to reload FROM: without the guard it
        // silently clears the scene, which reads as a crash rather than a no-op.
        {
            ModuleScene* scene = editorContext->GetScene();
            const bool hasFile = scene && !scene->GetCurrentScenePath().empty();

            ImGui::BeginDisabled(!hasFile);
            if (ImGui::MenuItem("Reload Scene"))
                scene->LoadSceneAsync(scene->GetCurrentScenePath());
            ImGui::EndDisabled();

            if (!hasFile && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("This scene has never been saved, so there is\nnothing on disk to reload from.");
        }

        // Distinct from New Scene, which is why both are here: New Scene also renames
        // the scene to Untitled and forgets its path, so the next Ctrl+S prompts for a
        // file. This empties the scene and KEEPS its identity -- so saving afterwards
        // writes the empty scene over the one it came from, which is the point, and
        // also why it confirms first.
        if (ImGui::MenuItem("Clear Scene"))
            openClearScene = true;

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Deletes every GameObject but keeps the scene's name and file.");

        ImGui::SeparatorText("Save");


        if (ImGui::MenuItem("Save", "Ctrl+S"))
        {
            saveToCurrent(openSaveAs);
        }

        if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S"))
        {
            openSaveAs = true;
        }

        ImGui::SeparatorText("Build");

        if (ImGui::MenuItem("Build"))
        {
            ModuleScene* scene = editorContext->GetScene();
            if (scene->HasCurrentScenePath())
            {
                scene->SaveScene(scene->GetCurrentScenePath());
                m_buildOutputPath   = GetDefaultBuildPath();
                m_buildLaunchAfter  = false;
                m_buildStartupScene = AssetPathToLibraryScene(scene->GetCurrentScenePath());
                m_openBuild         = true;
                editorContext->GetGameExporter()->StartExport({ m_buildOutputPath, m_buildStartupScene, false });
            }
            else
            {
                openSaveAs = true;
            }
        }

        if (ImGui::MenuItem("Build & Run"))
        {
            ModuleScene* scene = editorContext->GetScene();
            if (scene->HasCurrentScenePath())
            {
                scene->SaveScene(scene->GetCurrentScenePath());
                m_buildOutputPath   = GetDefaultBuildPath();
                m_buildLaunchAfter  = true;
                m_buildStartupScene = AssetPathToLibraryScene(scene->GetCurrentScenePath());
                m_openBuildAndRun   = true;
                editorContext->GetGameExporter()->StartExport({ m_buildOutputPath, m_buildStartupScene, true });
            }
            else
            {
                openSaveAs = true;
            }
        }

        if (ImGui::MenuItem("Build Settings..."))
        {
            if (m_buildOutputPath.empty())
                m_buildOutputPath = GetDefaultBuildPath();
            m_openBuildSettings = true;
        }


        ImGui::SeparatorText("Exit");

        // Quits through SDL_EVENT_QUIT, the same path window-close and Alt-F4 already
        // take: ModuleInput turns that event into UpdateStatus::STOP, so the shutdown
        // order is identical however the user asked to leave. Returning STOP from here
        // is not an option anyway -- this is an editor WINDOW, not a module, and
        // nothing it returns reaches Application::Update.
        //
        // Unsaved work is NOT prompted for, which matches every other exit route
        // today. Worth adding once, for all of them, rather than only for this one.
        if (ImGui::MenuItem("Exit", "Alt+F4"))
        {
            SDL_Event quit{};
            quit.type = SDL_EVENT_QUIT;
            SDL_PushEvent(&quit);
        }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit"))
    {
        ImGui::SeparatorText("Editor");

        // Writes the CURRENT arrangement -- window positions, sizes, docking, which
        // tab is selected in each node -- as the layout the editor opens with.
        //
        // Let ImGui write it rather than hand-editing the file: the dock nodes are
        // keyed by tab-ID hashes that are not derivable from the window names, so a
        // hand-edit can only safely change the parts that are not hashes.
        if (ImGui::MenuItem("Save editor configuration"))
        {
            ImGui::SaveIniSettingsToDisk(kEditorLayoutFile);
            NOUS_INFO("[Editor] Saved the current layout to '%s'.", kEditorLayoutFile);
        }

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Makes the current window arrangement the one the editor opens with.\n"
                              "Takes effect on the next launch.");

        // NO "Load editor configuration" counterpart, deliberately.
        //
        // LoadIniSettingsFromDisk mid-session rebuilds the dock TREE from the file,
        // but a window that already exists keeps its current dock association --
        // ImGui applies a window's settings when the window is CREATED, not on every
        // load. The result is a half-applied layout, and the node-editor windows show
        // it worst because their canvas sizes itself from the host window.
        //
        // Nothing is lost: startup already loads this file (ModuleEditor::Awake
        // copies it over imgui.ini), so the saved layout is what the editor opens
        // with either way. An in-session reset would need the dock tree torn down and
        // rebuilt through the DockBuilder API, which is a feature rather than a menu
        // item -- and a Load that corrupts the layout is worse than no Load.

        ImGui::SeparatorText("Other");

        if (ImGui::MenuItem("Preferences"))
        {
        }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View"))
    {
        // Moved out of File: it changes how the editor LOOKS, not what happens to
        // the project, which is what every other File item does.
        if (ImGui::BeginMenu("Theme"))
        {
            if (ImGui::MenuItem("Classic"))
            {
                ImGui::StyleColorsClassic();
            }
            if (ImGui::MenuItem("Light (please don't)"))
            {
                ImGui::StyleColorsLight();
            }
            if (ImGui::MenuItem("Dark"))
            {
                ImGui::StyleColorsDark();
            }

            ImGui::EndMenu();
        }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Renderer"))
    {
        if (ImGui::MenuItem("Reload Shaders", "Ctrl+R"))
        {
            editorContext->GetRendererFrontend()->ReloadAllShaders();
        }

        ImGui::Separator();

        bool showBB = editorContext->GetRendererFrontend()->showBoundingBoxes;
        if (ImGui::MenuItem("Bounding Boxes", nullptr, showBB))
            editorContext->GetRendererFrontend()->showBoundingBoxes = !showBB;

        // Off by default and per-vertex on the SELECTED object only, so it is the
        // one debug overlay worth turning on deliberately.
        bool showNormals = editorContext->GetRendererFrontend()->showNormals;
        if (ImGui::MenuItem("Normals", nullptr, showNormals))
            editorContext->GetRendererFrontend()->showNormals = !showNormals;

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("GameObject"))
    {
        ModuleScene* scene = editorContext->GetScene();
        Scene* activeScene = scene ? scene->activeScene : nullptr;

        // Everything here creates at the scene ROOT and selects the result, so the
        // Inspector is already showing the new object. Moved out of the Hierarchy's
        // right-click menu: creating an object is not something you do TO the
        // hierarchy, and hiding it behind a right-click on empty space made it
        // findable only by people who already knew it was there.
        ImGui::BeginDisabled(activeScene == nullptr);

        if (ImGui::MenuItem("Create Empty"))
        {
            GameObject go = activeScene->CreateGameObject("GameObject", nullptr);
            scene->SetSelection(go);
        }

        if (ImGui::MenuItem("Create Camera"))
        {
            GameObject go = activeScene->CreateGameObject("Main Camera", nullptr);
            auto& cam = go.AddComponent<CCamera>();
            cam.isMainCamera = true;
            scene->SetSelection(go);
        }

        if (ImGui::MenuItem("Create Light"))
        {
            GameObject go = activeScene->CreateGameObject("Directional Light", nullptr);
            go.AddComponent<CLight>();
            scene->SetSelection(go);
        }

        ImGui::Separator();

        if (ImGui::BeginMenu("Mesh"))
        {
            // SHAPE ONLY -- none of these do anything yet, and they are disabled
            // rather than silently inert so that is visible. An item with an empty
            // body looks implemented and is how this menu bar accumulated a Project
            // section, an Exit that did not exit and a Windows menu that did nothing.
            //
            // Implementing one means generating its vertices and indices into a
            // ResourceMesh, which the engine cannot currently do: every mesh today
            // arrives through ModelParser from a file on disk. The wireframe Cube /
            // Sphere / Cone in the renderer are LINE_LIST debug buffers owned by the
            // backend, not assets, so they are not a shortcut to this.
            ImGui::BeginDisabled(true);

            ImGui::MenuItem("Cube");
            ImGui::MenuItem("Sphere");
            ImGui::MenuItem("Plane");
            ImGui::MenuItem("Cylinder");
            ImGui::MenuItem("Cone");
            ImGui::MenuItem("Capsule");
            ImGui::MenuItem("Torus");

            ImGui::EndDisabled();

            ImGui::EndMenu();
        }

        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("Not implemented yet -- primitive meshes have to be\n"
                              "generated, and every mesh today comes from a file.");

        ImGui::EndDisabled();

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Assets"))
    {
        // Moved off the Assets Browser's own menu bar. These act on the PROJECT --
        // they re-stage files, rebuild Library/ and re-import -- so they belong
        // where project-wide actions live, not behind a menu inside one panel.
        //
        // Their state still lives on the browser, which is what the lookup is for:
        // the busy flag, the current directory and the item list are all its.
        auto* browser = static_cast<AssetsBrowser*>(editorContext->GetEditorWindow("Assets"));
        auto* resourceManager = editorContext->GetResourceManager();
        auto* jobSystem       = editorContext->GetJobSystem();

        ImGui::BeginDisabled(browser == nullptr || jobSystem == nullptr);

        // One flag for all three, because they all rebuild the same thing and
        // overlapping them would have two jobs writing Library/ at once.
        const bool busy = browser && browser->m_isRegeneratingLibrary.load();

        ImGui::BeginDisabled(busy);

        if (ImGui::MenuItem("Refresh Assets"))
        {
            jobSystem->SubmitJob([browser]()
            {
                // Re-runs the CMake target that stages Assets/ into the binary dir.
                // Staging is CONFIGURE-time otherwise, so without this an edited
                // asset never reaches a build that runs from bin/.
                std::system("cmake --build ./ --target CopyAssets");
                browser->AddItemsFromDirectory(browser->current_directory);
            }, "Refresh Assets");
        }

        // The one genuinely new action: ScanAndImportAssets walks Assets/ and
        // imports whatever is new or changed, WITHOUT throwing away Library/ the way
        // Regenerate does. It was exported with no caller anywhere in the editor.
        if (ImGui::MenuItem("Import New Assets"))
        {
            if (resourceManager)
            {
                browser->m_isRegeneratingLibrary = true;
                jobSystem->SubmitJob([browser, resourceManager]()
                {
                    resourceManager->ScanAndImportAssets();
                    browser->m_isRegeneratingLibrary = false;
                    browser->m_dirChanged.store(true, std::memory_order_release);
                }, "Import New Assets");
            }
        }

        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("Imports assets that are new or changed.\n"
                              "Keeps everything already in Library/.");

        if (ImGui::MenuItem(busy ? "Regenerating Library..." : "Regenerate Library"))
        {
            if (resourceManager)
            {
                browser->m_isRegeneratingLibrary = true;
                jobSystem->SubmitJob([browser, resourceManager]()
                {
                    resourceManager->RegenerateLibrary();
                    browser->m_isRegeneratingLibrary = false;
                    browser->m_dirChanged.store(true, std::memory_order_release);
                }, "Regenerate Library");
            }
        }

        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("Discards Library/ and re-imports everything from scratch.\n"
                              "The fix for a stale or corrupt cache.");

        ImGui::EndDisabled();

        ImGui::Separator();

        // View-only, unlike the three above: it empties the browser's listing, not
        // anything on disk.
        if (ImGui::MenuItem("Clear Items"))
            browser->ClearItems();

        ImGui::EndDisabled();

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Scripts"))
    {
        ModuleScene* scene = editorContext->GetScene();
        auto* jobSystem    = editorContext->GetJobSystem();

        ImGui::BeginDisabled(scene == nullptr || jobSystem == nullptr);

        // On a WORKER, because the slow part is a full MSVC compile:
        // ReloadScriptLibrary shells out to RebuildScripts.bat through
        // CreateProcessW, so doing this inline freezes the editor for seconds.
        //
        // Safe off the main thread by design rather than by luck: CScript::
        // ClearInstances sets the m_reloading atomic before destroying anything, and
        // OnUpdate / LateUpdate return early while it is set, so the main thread
        // stops touching script instances for the duration. ScriptManager guards its
        // component list with its own mutex on both halves.
        //
        // Note this is only safe for work with that flag behind it -- it is NOT a
        // precedent for running other scene mutation on a worker.
        if (ImGui::MenuItem("Recompile Scripts"))
            jobSystem->SubmitJob([scene] { scene->RecompileScripts(); }, "Scripts Hot-Reload");

        ImGui::EndDisabled();

        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("Rebuilds Scripts.dll and reloads it, keeping each\n"
                              "script's component and its Inspector values.\n"
                              "Runs in the background -- watch the Console for the result.");

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Windows"))
    {
        // DERIVED from the registered windows, never hand-listed. The list this
        // replaced was written once and never revisited: every entry had an empty
        // body, so closing a window by accident was permanent, and it named windows
        // that do not exist (Application, Assimp Log, Navigation, File Explorer,
        // Library) while omitting every window added since (Text Editor, Audio Graph
        // Editor, Audio Mixer, Animation Controller, Multithreading, Job Queue).
        //
        // Now adding a window to ModuleEditor is the whole job -- there is no second
        // place to remember.
        bool any = false;

        editorContext->ForEachEditorWindow([&any](IEditorWindow& window)
        {
            if (!window.ShowInWindowsMenu())
                return;

            any = true;

            // The tick mirrors the window's own open flag, which the title bar's X
            // also writes -- so the menu always reports the real state rather than a
            // copy of it that can drift.
            bool open = window.IsOpen();
            if (ImGui::MenuItem(window.GetTitle(), nullptr, &open))
            {
                if (open) window.Open();
                else      window.Close();
            }
        });

        if (!any)
            ImGui::TextDisabled("No windows registered.");

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Help"))
    {
        if (ImGui::MenuItem("About"))
        {
        }

        if (ImGui::MenuItem("Repository"))
        {
            RequestBrowser("https://github.com/francesctr4/Nous-Engine");
        }

        // All three go through RequestBrowser, exactly as Repository already did --
        // they were empty only because nobody had written the URL down.
        if (ImGui::MenuItem("Documentation"))
        {
            RequestBrowser("https://github.com/francesctr4/Nous-Engine/wiki");
        }

        if (ImGui::MenuItem("Releases"))
        {
            RequestBrowser("https://github.com/francesctr4/Nous-Engine/releases");
        }

        if (ImGui::MenuItem("Bug report"))
        {
            RequestBrowser("https://github.com/francesctr4/Nous-Engine/issues/new");
        }

        ImGui::EndMenu();
    }

    // -----------------------------------------------------------------------
    // Centered simulation toolbar — Play / Pause / Stop / Step
    // -----------------------------------------------------------------------
    {
        const SimulationState state = editorContext->GetScene()->GetSimulationState();

        constexpr float buttonW = 52.0f;
        constexpr float spacing = 4.0f;
        constexpr float totalW = buttonW * 4.0f + spacing * 3.0f;
        const float centerX = (ImGui::GetWindowWidth() - totalW) * 0.5f;
        ImGui::SetCursorPosX(centerX);

        // Play button — disabled while already playing
        const bool canPlay = (state == SimulationState::STOPPED);
        if (!canPlay) ImGui::BeginDisabled();
        if (ImGui::Button("Play", {buttonW, 0}))
            editorContext->GetScene()->PressPlay();
        if (!canPlay) ImGui::EndDisabled();

        ImGui::SameLine(0.0f, spacing);

        // Pause button — disabled when stopped; highlighted when paused
        const bool canPause = (state != SimulationState::STOPPED);
        const bool isPaused = (state == SimulationState::PAUSED);
        if (!canPause) ImGui::BeginDisabled();
        if (isPaused) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.65f, 0.10f, 1.0f));
        if (ImGui::Button("Pause", {buttonW, 0}))
            editorContext->GetScene()->PressPause();
        if (isPaused) ImGui::PopStyleColor();
        if (!canPause) ImGui::EndDisabled();

        ImGui::SameLine(0.0f, spacing);

        // Stop button — disabled when already stopped
        const bool canStop = (state != SimulationState::STOPPED);
        if (!canStop) ImGui::BeginDisabled();
        if (ImGui::Button("Stop", {buttonW, 0}))
            editorContext->GetScene()->PressStop();
        if (!canStop) ImGui::EndDisabled();

        ImGui::SameLine(0.0f, spacing);

        // Step button — only available while paused
        const bool canStep = (state == SimulationState::PAUSED);
        if (!canStep) ImGui::BeginDisabled();
        if (ImGui::Button("Step", {buttonW, 0}))
            editorContext->GetScene()->PressStep();
        if (!canStep) ImGui::EndDisabled();
    }
}

void MainMenuBar::DrawBuildModal()
{
    static bool s_scrollToBottom = false;

    constexpr int c_totalSteps = 7; // steps 1-6 = pipeline work, step 7 = patch config
    GameExporter* exporter = editorContext->GetGameExporter();

    if (m_openBuild || m_openBuildAndRun)
    {
        m_buildLog.clear();
        m_buildDone      = false;
        m_buildSuccess   = false;
        s_scrollToBottom = true;
        ImGui::OpenPopup(kBuildModal);
    }

    if (exporter->IsRunning() || m_buildModalOpen)
    {
        exporter->DrainLog(m_buildLog);

        bool success = false;
        if (exporter->PollDone(success))
        {
            m_buildDone    = true;
            m_buildSuccess = success;
        }
    }

    const char* title = m_buildDone
        ? (m_buildSuccess ? "Build Complete###BuildGameModal"
                          : "Build Failed###BuildGameModal")
        : "Building Game...###BuildGameModal";

    ImGui::SetNextWindowSize(ImVec2(660, 420), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_NoResize))
    {
        m_buildModalOpen = true;

        if (!m_buildDone)
        {
            const float progress = static_cast<float>(exporter->GetCurrentStep()) /
                                   static_cast<float>(c_totalSteps);
            ImGui::ProgressBar(progress, ImVec2(-1.0f, 0.0f));
            ImGui::Spacing();
        }

        const float logHeight = m_buildDone ? -52.0f : -34.0f;
        ImGui::BeginChild("##buildlog", ImVec2(-1.0f, logHeight), true);
        for (const auto& line : m_buildLog)
            ImGui::TextUnformatted(line.c_str());
        if (s_scrollToBottom || ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        {
            ImGui::SetScrollHereY(1.0f);
            s_scrollToBottom = false;
        }
        ImGui::EndChild();

        ImGui::Spacing();

        if (!m_buildDone)
        {
            const bool cancelling = !exporter->IsRunning() && m_buildModalOpen;
            if (cancelling) ImGui::BeginDisabled();
            if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
                exporter->Cancel();
            if (cancelling) ImGui::EndDisabled();
        }
        else
        {
            if (m_buildSuccess)
                ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.2f, 1.0f),
                                   "Build succeeded -> %s  |  Startup: %s",
                                   m_buildOutputPath.c_str(), m_buildStartupScene.c_str());
            else
                ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1.0f),
                                   "Build FAILED — see log above.");

            ImGui::Spacing();

            if (m_buildSuccess)
            {
                if (ImGui::Button("Open Folder", ImVec2(120.0f, 0.0f)))
                {
#ifdef _WIN32
                    ShellExecuteA(nullptr, "explore",
                                  m_buildOutputPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#endif
                }
                ImGui::SameLine();
            }

            if (ImGui::Button("OK", ImVec2(80.0f, 0.0f)))
            {
                m_buildModalOpen = false;
                m_buildDone      = false;
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::EndPopup();
    }
    else
    {
        m_buildModalOpen = false;
    }
}

void MainMenuBar::DrawBuildSettingsPopup()
{
    if (m_openBuildSettings)
        ImGui::OpenPopup(kBuildSettingsPopup);

    ImGui::SetNextWindowSize(ImVec2(520.0f, 0.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopup(kBuildSettingsPopup))
    {
        static char s_pathBuf[512] = {};
        static std::vector<std::string> s_sceneNames;
        static int s_selectedScene = -1;

        if (m_openBuildSettings)
        {
            std::snprintf(s_pathBuf, sizeof(s_pathBuf), "%s", m_buildOutputPath.c_str());

            // Scene list comes from the manifest (readable names) rather than the
            // Library/Scenes/ directory, which now holds UID-keyed <uid>.nous files.
            s_sceneNames = ImportPipeline::GetSceneNames();
            std::sort(s_sceneNames.begin(), s_sceneNames.end());

            // Default selection: match the current startup scene by name. The stored
            // path is "Library/Scenes/<name>.nous", so compare against its stem.
            s_selectedScene = -1;
            const std::string currentName = std::filesystem::path(m_buildStartupScene).stem().string();
            for (int i = 0; i < static_cast<int>(s_sceneNames.size()); ++i)
            {
                if (s_sceneNames[i] == currentName) { s_selectedScene = i; break; }
            }
        }

        // ── Output path ──────────────────────────────────────────────────
        ImGui::TextUnformatted("Output path:");
        ImGui::SetNextItemWidth(380.0f);
        ImGui::InputText("##buildSettingsPath", s_pathBuf, sizeof(s_pathBuf));
        ImGui::SameLine();

        if (ImGui::Button("Browse"))
        {
#ifdef _WIN32
            BROWSEINFOA bi   = {};
            bi.lpszTitle     = "Select output folder";
            bi.ulFlags       = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
            LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
            if (pidl)
            {
                char picked[MAX_PATH];
                if (SHGetPathFromIDListA(pidl, picked))
                    std::snprintf(s_pathBuf, sizeof(s_pathBuf), "%s", picked);
                CoTaskMemFree(pidl);
            }
#endif
        }

        ImGui::TextDisabled("Default: <engine_dir>/Delivery/Game");
        ImGui::Spacing();

        // ── Startup scene ────────────────────────────────────────────────
        ImGui::TextUnformatted("Startup scene:");
        ImGui::SetNextItemWidth(460.0f);
        const char* previewLabel = (s_selectedScene >= 0 && s_selectedScene < static_cast<int>(s_sceneNames.size()))
            ? s_sceneNames[s_selectedScene].c_str()
            : "(select a scene)";
        if (ImGui::BeginCombo("##startupScene", previewLabel))
        {
            for (int i = 0; i < static_cast<int>(s_sceneNames.size()); ++i)
            {
                const bool selected = (s_selectedScene == i);
                if (ImGui::Selectable(s_sceneNames[i].c_str(), selected))
                    s_selectedScene = i;
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::Spacing();

        // ── Buttons ──────────────────────────────────────────────────────
        const bool pathValid  = s_pathBuf[0] != '\0';
        const bool sceneValid = s_selectedScene >= 0;
        const bool canBuild   = pathValid && sceneValid;

        if (!canBuild) ImGui::BeginDisabled();
        if (ImGui::Button("Build", ImVec2(120.0f, 0.0f)))
        {
            ModuleScene* scene = editorContext->GetScene();
            if (scene->HasCurrentScenePath())
                scene->SaveScene(scene->GetCurrentScenePath());

            m_buildOutputPath   = s_pathBuf;
            m_buildStartupScene = "Library/Scenes/" + s_sceneNames[s_selectedScene] + ".nous";
            m_buildLaunchAfter  = false;
            m_openBuild         = true;
            editorContext->GetGameExporter()->StartExport({ m_buildOutputPath, m_buildStartupScene, false });
            ImGui::CloseCurrentPopup();
        }
        if (!canBuild) ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)) ||
            ImGui::IsKeyPressed(ImGuiKey_Escape))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }
}

void MainMenuBar::FinishUpdate()
{
    ModuleScene* scene = editorContext->GetScene();

    // -------------------------------------------------------------------
    // Deferred popup opens (must happen outside BeginMainMenuBar stack).
    // -------------------------------------------------------------------
    if (openSaveAs) ImGui::OpenPopup(kSaveAsPopup);
    if (openOpen) ImGui::OpenPopup(kOpenPopup);
    if (openNewScene) ImGui::OpenPopup(kNewScenePopup);
    if (openClearScene) ImGui::OpenPopup(kClearScenePopup);

    DrawBuildSettingsPopup();
    DrawBuildModal();

    // -------------------------------------------------------------------
    // Save Scene As... modal
    // -------------------------------------------------------------------
    if (ImGui::BeginPopupModal(kSaveAsPopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        static char nameBuf[128] = {0};

        // Pre-fill with current scene's filename stem on first open.
        if (openSaveAs)
        {
            const std::string& curPath = scene->GetCurrentScenePath();
            if (!curPath.empty())
            {
                const std::string stem = std::filesystem::path(curPath).stem().string();
                std::snprintf(nameBuf, sizeof(nameBuf), "%s", stem.c_str());
            }
            else
            {
                std::snprintf(nameBuf, sizeof(nameBuf), "%s", "NewScene");
            }
            ImGui::SetKeyboardFocusHere();
        }

        const std::string saveDir = editorContext->GetAssetsBrowserDirectory();
        ImGui::Text("Save to: %s/", saveDir.c_str());
        const bool enterPressed = ImGui::InputText("##sceneName", nameBuf, sizeof(nameBuf),
                                                   ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        ImGui::TextUnformatted(kSceneExt);

        const std::string trimmed = [&]()
        {
            std::string s = nameBuf;
            while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(s.begin());
            while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();
            return s;
        }();

        const bool nameValid = !trimmed.empty() &&
            trimmed.find_first_of("\\/:*?\"<>|") == std::string::npos;

        const std::string targetPath = nameValid
                                           ? (saveDir + "/" + trimmed + kSceneExt)
                                           : std::string();
        const bool overwriting = nameValid && nous::engine::filesystem::Exists(targetPath);

        if (!nameValid)
            ImGui::TextColored(ImVec4(0.9f, 0.4f, 0.4f, 1.0f), "Enter a valid filename.");
        else if (overwriting)
            ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f), "File exists — will be overwritten.");
        else
            ImGui::TextUnformatted(" ");

        if (!nameValid) ImGui::BeginDisabled();
        const bool saveClicked = ImGui::Button("Save", ImVec2(120, 0));
        if (!nameValid) ImGui::EndDisabled();
        const bool doSave = nameValid && (enterPressed || saveClicked);
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape))
            ImGui::CloseCurrentPopup();

        if (doSave)
        {
            scene->SaveScene(targetPath);
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    // -------------------------------------------------------------------
    // Open Scene modal
    // -------------------------------------------------------------------
    if (ImGui::BeginPopupModal(kOpenPopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        // Scan all of Assets/ recursively each frame the popup is open — keeps the
        // list fresh if the user drops a scene in externally or saves to a subfolder.
        static int selectedIdx = -1;
        std::vector<std::string> sceneFiles;

        std::error_code ec;
        if (std::filesystem::exists("Assets", ec) && std::filesystem::is_directory("Assets", ec))
        {
            for (const auto& entry : std::filesystem::recursive_directory_iterator("Assets", ec))
            {
                if (!entry.is_regular_file()) continue;
                if (entry.path().extension().string() == kSceneExt)
                    sceneFiles.push_back(entry.path().generic_string());
            }
        }

        if (openOpen) selectedIdx = -1;

        ImGui::TextUnformatted("Select a scene:");
        ImGui::Separator();

        if (sceneFiles.empty())
        {
            ImGui::TextDisabled("(no %s files found)", kSceneExt);
        }
        else
        {
            if (ImGui::BeginListBox("##sceneList", ImVec2(360, 200)))
            {
                for (int i = 0; i < (int)sceneFiles.size(); ++i)
                {
                    const bool isSelected = (selectedIdx == i);
                    if (ImGui::Selectable(sceneFiles[i].c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick))
                    {
                        selectedIdx = i;
                        if (ImGui::IsMouseDoubleClicked(0))
                        {
                            scene->LoadSceneAsync(sceneFiles[selectedIdx]);
                            ImGui::CloseCurrentPopup();
                        }
                    }
                    if (isSelected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndListBox();
            }
        }

        const bool canOpen = (selectedIdx >= 0 && selectedIdx < (int)sceneFiles.size());
        if (!canOpen) ImGui::BeginDisabled();
        if (ImGui::Button("Open", ImVec2(120, 0)))
        {
            scene->LoadSceneAsync(sceneFiles[selectedIdx]);
            ImGui::CloseCurrentPopup();
        }
        if (!canOpen) ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }

    // -------------------------------------------------------------------
    // New Scene confirmation modal
    // -------------------------------------------------------------------
    if (ImGui::BeginPopupModal(kNewScenePopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("Create a new empty scene?");
        ImGui::TextDisabled("Unsaved changes in the active scene will be lost.");
        ImGui::Separator();

        if (ImGui::Button("Create", ImVec2(120, 0)))
        {
            scene->NewScene();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal(kClearScenePopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("Delete every GameObject in this scene?");
        ImGui::TextDisabled("The scene keeps its name and file, so saving after this\n"
                            "writes the empty scene over it. There is no undo.");
        ImGui::Separator();

        if (ImGui::Button("Clear", ImVec2(120, 0)))
        {
            // Drained first, for the same reason NewScene drains: a spawn or an async
            // load already in flight finishes on a LATER frame and hands its objects
            // to the main thread, which would repopulate the scene moments after it
            // was cleared. ClearScene does not drain on its own -- it must not, since
            // waiting from a worker of the same system deadlocks -- so the call site
            // that can guarantee it is on the main thread does it.
            if (auto* jobSystem = editorContext->GetJobSystem())
                jobSystem->WaitForPendingJobs();

            scene->ClearScene();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }
}
