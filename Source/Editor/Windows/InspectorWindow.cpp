#include "InspectorWindow.h"

#include "Core/Application.h"
#include "Modules/ModuleScene.h"
#include "ECS/GameObject.h"

InspectorWindow::InspectorWindow(const char* title, bool start_open)
        : IEditorWindow(title, nullptr, start_open) {
    Init();
}

void InspectorWindow::Init()
{

}

void InspectorWindow::Draw() {
    if (*p_open)
    {
        if (ImGui::Begin(title, p_open))
        {
            if (GameObject* go = External->scene->selectedGameObject)
            {
                // Display object name
                ImGui::Text("GameObject: %s", go->GetName().c_str());

                // Editable name (optional)
                static char buffer[256];
                strncpy_s(buffer, go->GetName().c_str(), sizeof(buffer));
                if (ImGui::InputText("Name", buffer, sizeof(buffer))) {
                    go->SetName(buffer);
                }
            } else
            {
                ImGui::Text("No GameObject selected.");
            }
        }
        ImGui::End();
    }
}
