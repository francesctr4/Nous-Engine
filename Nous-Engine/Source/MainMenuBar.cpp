#include "MainMenuBar.h"

MainMenuBar::MainMenuBar(const char* title, bool start_open)
    : IEditorWindow(title, nullptr, start_open) 
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

			if (ImGui::MenuItem("Scene")) {

				

			}

			if (ImGui::MenuItem("Game")) {

				

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

		ImGui::EndMainMenuBar();

	}
}