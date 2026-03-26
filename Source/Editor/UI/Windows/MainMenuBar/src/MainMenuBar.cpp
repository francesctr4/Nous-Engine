#include "Editor/UI/Windows/MainMenuBar/include/MainMenuBar.h"

#include "imgui.h"
#include "Engine/Core/Application.h"
#include "Engine/Modules/ModuleScene/include/ModuleScene.h"

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
	if (ImGui::BeginMainMenuBar()) {

		if (ImGui::BeginMenu("File")) {

			ImGui::SeparatorText("Scene");

			if (ImGui::MenuItem("New Scene")) {



			}

			if (ImGui::MenuItem("Open Scene")) {



			}

			ImGui::SeparatorText("Save");


			if (ImGui::MenuItem("Save", "Ctrl+S"))
			{
				
			}

			if (ImGui::MenuItem("Save As...", "Ctrl+LAlt+S"))
			{
				
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
				//External->editor->GetEditorWindowByName("Scene")->Open();
			}

			if (ImGui::MenuItem("Game")) 
			{
				//External->editor->GetEditorWindowByName("Game")->Open();
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
			const SimulationState state = External->GetScene()->GetSimulationState();

			constexpr float buttonW  = 52.0f;
			constexpr float spacing  = 4.0f;
			constexpr float totalW   = buttonW * 4.0f + spacing * 3.0f;
			const float     centerX  = (ImGui::GetWindowWidth() - totalW) * 0.5f;
			ImGui::SetCursorPosX(centerX);

			// Play button — disabled while already playing
			const bool canPlay = (state == SimulationState::STOPPED);
			if (!canPlay) ImGui::BeginDisabled();
			if (ImGui::Button("Play", { buttonW, 0 }))
				External->GetScene()->PressPlay();
			if (!canPlay) ImGui::EndDisabled();

			ImGui::SameLine(0.0f, spacing);

			// Pause button — disabled when stopped; highlighted when paused
			const bool canPause  = (state != SimulationState::STOPPED);
			const bool isPaused  = (state == SimulationState::PAUSED);
			if (!canPause) ImGui::BeginDisabled();
			if (isPaused)  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.65f, 0.10f, 1.0f));
			if (ImGui::Button("Pause", { buttonW, 0 }))
				External->GetScene()->PressPause();
			if (isPaused)  ImGui::PopStyleColor();
			if (!canPause) ImGui::EndDisabled();

			ImGui::SameLine(0.0f, spacing);

			// Stop button — disabled when already stopped
			const bool canStop = (state != SimulationState::STOPPED);
			if (!canStop) ImGui::BeginDisabled();
			if (ImGui::Button("Stop", { buttonW, 0 }))
				External->GetScene()->PressStop();
			if (!canStop) ImGui::EndDisabled();

			ImGui::SameLine(0.0f, spacing);

			// Step button — only available while paused
			const bool canStep = (state == SimulationState::PAUSED);
			if (!canStep) ImGui::BeginDisabled();
			if (ImGui::Button("Step", { buttonW, 0 }))
				External->GetScene()->PressStep();
			if (!canStep) ImGui::EndDisabled();
		}

		ImGui::EndMainMenuBar();

	}
}