#include "Properties.h"

Properties::Properties(const char* title, bool start_open) 
    : IEditorWindow(title, nullptr, start_open) 
{
    Init();
}

void Properties::Init()
{

}

void Properties::Draw()
{
    if (*p_open) 
    { 
        if (ImGui::Begin(title, p_open)) 
        {
            ImGui::Text("Properties content here...");
        }
        ImGui::End();
    }
}
