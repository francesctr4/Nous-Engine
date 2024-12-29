#include "Assets.h"

Assets::Assets(const char* title, bool start_open)
    : IEditorWindow(title, nullptr, start_open) {}

void Assets::Draw()
{
    if (*p_open)
    {
        if (ImGui::Begin(title, p_open))
        {
            ImGui::Text("Here are the assets.");
        }
        ImGui::End();
    }
}