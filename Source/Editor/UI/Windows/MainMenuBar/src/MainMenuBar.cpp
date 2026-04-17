#include "Editor/UI/Windows/MainMenuBar/include/MainMenuBar.h"

#include "imgui.h"
#include "Engine/Modules/ModuleScene/include/ModuleScene.h"
#include "Engine/Renderer/Frontend/RendererFrontend.h"
#include "Engine/Core/FileSystem/FileSystem.h"

#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#include <shellapi.h>
#elif defined(__APPLE__)
#include <cstdlib>
    #include <unistd.h>
#elif defined(__linux__)
    #include <cstdlib>
    #include <unistd.h>
    #include <sys/wait.h>
#endif

static bool RequestBrowser(const char* url)
{
#ifdef _WIN32
	HINSTANCE result = ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOWNORMAL);
	return (INT_PTR)result > 32; // ShellExecute devuelve > 32 si es exitoso
#elif defined(__APPLE__)
	pid_t pid = fork();
    if (pid == 0) {
        execl("/usr/bin/open", "open", url, nullptr);
        exit(1);
    }
    return pid > 0;
#elif defined(__linux__)
    pid_t pid = fork();
    if (pid == 0) {
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
	Init();
}

void MainMenuBar::Init()
{

}

void MainMenuBar::Draw()
{
	static constexpr const char* kScenesDir     = "Assets/Scenes";
	static constexpr const char* kSceneExt      = ".nous";
	static constexpr const char* kSaveAsPopup   = "Save Scene As";
	static constexpr const char* kOpenPopup     = "Open Scene";
	static constexpr const char* kNewScenePopup = "New Scene";

	ModuleScene* scene = editorContext->GetScene();

	// Deferred popup opens — must happen outside BeginMenu, after EndMainMenuBar.
	bool openSaveAs   = false;
	bool openOpen     = false;
	bool openNewScene = false;

	// Save to current path, or fall back to Save As if none set.
	auto saveToCurrent = [&](bool& triggerSaveAs)
	{
		if (scene->HasCurrentScenePath())
			scene->SaveScene(scene->GetCurrentScenePath());
		else
			triggerSaveAs = true;
	};

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

	if (ImGui::BeginMainMenuBar()) {

		if (ImGui::BeginMenu("File")) {

			ImGui::SeparatorText("Scene");

			if (ImGui::MenuItem("New Scene")) {
				openNewScene = true;
			}

			if (ImGui::MenuItem("Open Scene")) {
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

			if (ImGui::MenuItem("New Project")) {



			}

			if (ImGui::MenuItem("Open Project")) {



			}

			if (ImGui::MenuItem("Save Project")) {



			}

			ImGui::SeparatorText("Build");

			if (ImGui::MenuItem("Build")) 
			{
				//std::system("\"C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\MSBuild\\Current\\Bin\\msbuild.exe\" ..\\Nous-Engine.sln /p:Configuration=Release /m");
				//std::system("..\\build.bat");
			}

			if (ImGui::MenuItem("Build & Run")) {

				//std::system("\"C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\MSBuild\\Current\\Bin\\msbuild.exe\" ..\\Nous-Engine.sln /p:Configuration=Release /m");
				//std::system("..\\build.bat");
				//std::system("..\\Build\\Nous-Engine-v0.1\\Nous-Engine.exe");
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

		if (ImGui::BeginMenu("Edit")) {

			ImGui::SeparatorText("Editor");

			if (ImGui::MenuItem("Save editor configuration")) {

			}

			if (ImGui::MenuItem("Load editor configuration")) {



			}

			ImGui::SeparatorText("Other");

			if (ImGui::MenuItem("Preferences")) {



			}

			ImGui::EndMenu();

		}

		if (ImGui::BeginMenu("View")) {



			ImGui::EndMenu();

		}

		if (ImGui::BeginMenu("Renderer")) {

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

		if (ImGui::BeginMenu("GameObject")) {

			if (ImGui::MenuItem("Empty")) 
			{

				

			}

			ImGui::Separator();

			if (ImGui::MenuItem("Clear Scene")) 
			{

			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Windows")) {

			if (ImGui::MenuItem("Application")) {

				

			}

			if (ImGui::MenuItem("Console")) {

				

			}

			if (ImGui::MenuItem("Memory Leaks")) {

				



			}

			if (ImGui::MenuItem("Assimp Log")) {

				

			}

			if (ImGui::MenuItem("Hierarchy")) {

				

			}

			if (ImGui::MenuItem("Inspector")) {

				

			}

			if (ImGui::MenuItem("Navigation")) {

				

			}

			if (ImGui::MenuItem("Scene")) 
			{

			}

			if (ImGui::MenuItem("Game")) 
			{

			}

			if (ImGui::MenuItem("Resources")) {

				

			}

			if (ImGui::MenuItem("File Explorer")) {

				

			}

			if (ImGui::MenuItem("Assets")) {



			}

			if (ImGui::MenuItem("Library")) {

				

			}

			if (ImGui::MenuItem("Node Editor")) {

				

			}

			if (ImGui::MenuItem("Shader Editor")) {

				

			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Help")) {

			if (ImGui::MenuItem("About")) {

				

			}

			if (ImGui::MenuItem("Repository")) {

				RequestBrowser("https://github.com/francesctr4/Nous-Engine");

			}

			if (ImGui::MenuItem("Documentation")) {



			}

			if (ImGui::MenuItem("Releases")) {



			}

			if (ImGui::MenuItem("Bug report")) {



			}

			ImGui::EndMenu();

		}

		// -----------------------------------------------------------------------
		// Centered simulation toolbar — Play / Pause / Stop / Step
		// -----------------------------------------------------------------------
		{
			const SimulationState state = editorContext->GetScene()->GetSimulationState();

			constexpr float buttonW  = 52.0f;
			constexpr float spacing  = 4.0f;
			constexpr float totalW   = buttonW * 4.0f + spacing * 3.0f;
			const float     centerX  = (ImGui::GetWindowWidth() - totalW) * 0.5f;
			ImGui::SetCursorPosX(centerX);

			// Play button — disabled while already playing
			const bool canPlay = (state == SimulationState::STOPPED);
			if (!canPlay) ImGui::BeginDisabled();
			if (ImGui::Button("Play", { buttonW, 0 }))
				editorContext->GetScene()->PressPlay();
			if (!canPlay) ImGui::EndDisabled();

			ImGui::SameLine(0.0f, spacing);

			// Pause button — disabled when stopped; highlighted when paused
			const bool canPause  = (state != SimulationState::STOPPED);
			const bool isPaused  = (state == SimulationState::PAUSED);
			if (!canPause) ImGui::BeginDisabled();
			if (isPaused)  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.65f, 0.10f, 1.0f));
			if (ImGui::Button("Pause", { buttonW, 0 }))
				editorContext->GetScene()->PressPause();
			if (isPaused)  ImGui::PopStyleColor();
			if (!canPause) ImGui::EndDisabled();

			ImGui::SameLine(0.0f, spacing);

			// Stop button — disabled when already stopped
			const bool canStop = (state != SimulationState::STOPPED);
			if (!canStop) ImGui::BeginDisabled();
			if (ImGui::Button("Stop", { buttonW, 0 }))
				editorContext->GetScene()->PressStop();
			if (!canStop) ImGui::EndDisabled();

			ImGui::SameLine(0.0f, spacing);

			// Step button — only available while paused
			const bool canStep = (state == SimulationState::PAUSED);
			if (!canStep) ImGui::BeginDisabled();
			if (ImGui::Button("Step", { buttonW, 0 }))
				editorContext->GetScene()->PressStep();
			if (!canStep) ImGui::EndDisabled();
		}

		ImGui::EndMainMenuBar();

	}

	// -------------------------------------------------------------------
	// Deferred popup opens (must happen outside BeginMainMenuBar stack).
	// -------------------------------------------------------------------
	if (openSaveAs)   ImGui::OpenPopup(kSaveAsPopup);
	if (openOpen)     ImGui::OpenPopup(kOpenPopup);
	if (openNewScene) ImGui::OpenPopup(kNewScenePopup);

	// -------------------------------------------------------------------
	// Save Scene As... modal
	// -------------------------------------------------------------------
	if (ImGui::BeginPopupModal(kSaveAsPopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		static char nameBuf[128] = { 0 };

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

		ImGui::Text("Save to: %s/", kScenesDir);
		const bool enterPressed = ImGui::InputText("##sceneName", nameBuf, sizeof(nameBuf),
			ImGuiInputTextFlags_EnterReturnsTrue);
		ImGui::SameLine();
		ImGui::TextUnformatted(kSceneExt);

		const std::string trimmed = [&]() {
			std::string s = nameBuf;
			while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(s.begin());
			while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();
			return s;
		}();

		const bool nameValid = !trimmed.empty() &&
			trimmed.find_first_of("\\/:*?\"<>|") == std::string::npos;

		const std::string targetPath = nameValid
			? (std::string(kScenesDir) + "/" + trimmed + kSceneExt)
			: std::string();
		const bool overwriting = nameValid && NOUS_FileManager::Exists(targetPath);

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
		// Scan Assets/Scenes/*.nous each frame the popup is open — cheap, and
		// keeps the list fresh if the user drops a scene in externally.
		static int selectedIdx = -1;
		std::vector<std::string> sceneFiles;

		std::error_code ec;
		if (std::filesystem::exists(kScenesDir, ec) && std::filesystem::is_directory(kScenesDir, ec))
		{
			for (const auto& entry : std::filesystem::directory_iterator(kScenesDir, ec))
			{
				if (!entry.is_regular_file()) continue;
				if (entry.path().extension().string() == kSceneExt)
					sceneFiles.push_back(entry.path().filename().string());
			}
		}

		if (openOpen) selectedIdx = -1;

		ImGui::Text("Select a scene from %s/:", kScenesDir);
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
							const std::string chosen = std::string(kScenesDir) + "/" + sceneFiles[i];
							scene->LoadSceneAsync(chosen);
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
			const std::string chosen = std::string(kScenesDir) + "/" + sceneFiles[selectedIdx];
			scene->LoadSceneAsync(chosen);
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