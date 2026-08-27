#include "Editor/UI/Windows/MainMenuBar/include/MainMenuBar.h"
#include "Editor/GameExporter/include/GameExporter.h"

#include "imgui.h"
#include "Engine/Modules/ModuleScene/include/ModuleScene.h"
#include "Engine/Renderer/Frontend/RendererFrontend.h"
#include <FileSystem/FileSystem.h>
#include <ResourceManager/Runtime/ImportPipeline.h>

#include <SDL3/SDL.h>

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
    openSaveAs   = false;
    openOpen     = false;
    openNewScene = false;

    m_openBuild         = false;
    m_openBuildAndRun   = false;
    m_openBuildSettings = false;

    // Keyboard shortcuts (only when no modal dialog is open).
    if (!ImGui::IsPopupOpen(kSaveAsPopup) && !ImGui::IsPopupOpen(kOpenPopup) && !ImGui::IsPopupOpen(kNewScenePopup))
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

        ImGui::SeparatorText("Save");


        if (ImGui::MenuItem("Save", "Ctrl+S"))
        {
            saveToCurrent(openSaveAs);
        }

        if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S"))
        {
            openSaveAs = true;
        }

        ImGui::SeparatorText("Project");

        if (ImGui::MenuItem("New Project"))
        {
        }

        if (ImGui::MenuItem("Open Project"))
        {
        }

        if (ImGui::MenuItem("Save Project"))
        {
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

        ImGui::SeparatorText("Exit");

        if (ImGui::MenuItem("Exit"))
        {
        }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit"))
    {
        ImGui::SeparatorText("Editor");

        if (ImGui::MenuItem("Save editor configuration"))
        {
        }

        if (ImGui::MenuItem("Load editor configuration"))
        {
        }

        ImGui::SeparatorText("Other");

        if (ImGui::MenuItem("Preferences"))
        {
        }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View"))
    {
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

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("GameObject"))
    {
        if (ImGui::MenuItem("Empty"))
        {
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Clear Scene"))
        {
        }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Windows"))
    {
        if (ImGui::MenuItem("Application"))
        {
        }

        if (ImGui::MenuItem("Console"))
        {
        }

        if (ImGui::MenuItem("Memory Leaks"))
        {
        }

        if (ImGui::MenuItem("Assimp Log"))
        {
        }

        if (ImGui::MenuItem("Hierarchy"))
        {
        }

        if (ImGui::MenuItem("Inspector"))
        {
        }

        if (ImGui::MenuItem("Navigation"))
        {
        }

        if (ImGui::MenuItem("Scene"))
        {
        }

        if (ImGui::MenuItem("Game"))
        {
        }

        if (ImGui::MenuItem("Resources"))
        {
        }

        if (ImGui::MenuItem("File Explorer"))
        {
        }

        if (ImGui::MenuItem("Assets"))
        {
        }

        if (ImGui::MenuItem("Library"))
        {
        }

        if (ImGui::MenuItem("Node Editor"))
        {
        }

        if (ImGui::MenuItem("Shader Editor"))
        {
        }

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

        if (ImGui::MenuItem("Documentation"))
        {
        }

        if (ImGui::MenuItem("Releases"))
        {
        }

        if (ImGui::MenuItem("Bug report"))
        {
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
}
