#include "ThreadsWindow.h"

#include "NOUS_Multithreading.h"

Threads::Threads(const char* title, bool start_open)
    : IEditorWindow(title, nullptr, start_open)
{
    Init();
}

void Threads::Init()
{

}

void Threads::Draw()
{
    if (*p_open)
    {
        if (ImGui::Begin(title, p_open))
        {
            ImGui::Text("Registered Threads Info");
            ImGui::Separator();
            ImGui::Text("Max Threads: %u", NOUS_Multithreading::c_MAX_HARDWARE_THREADS);
            ImGui::Separator();
            ImGui::Text("Active Threads: %u", NOUS_Multithreading::registeredThreads.size());
            ImGui::Separator();

            for (const auto& Thread : NOUS_Multithreading::registeredThreads)
            {
                ImGui::Separator();
                ImGui::Text("Thread ID: %lu", Thread->ID);
                ImGui::Text("Thread Name: %s", Thread->name.c_str());
                ImGui::Text("Thread State: %s", NOUS_Multithreading::GetStringFromState(Thread->state).c_str());
                ImGui::Text("Elapsed Time: %.3f", Thread->executionTime.ReadSec());
                ImGui::Separator();
            }
            ImGui::Separator();
        }
        ImGui::End();
    }
}