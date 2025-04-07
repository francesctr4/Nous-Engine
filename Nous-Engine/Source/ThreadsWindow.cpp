#include "ThreadsWindow.h"

#include "Application.h"
#include "NOUS_JobSystem.h"
#include "NOUS_Multithreading.h"

#include <algorithm>

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
    if (!*p_open) return;

    ImGui::SetNextWindowSize(ImVec2(650, 400), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(title, p_open))
    {
        // System overview section
        ImGui::Text("Thread System Overview");
        ImGui::Separator();

        static int newSize = NOUS_Multithreading::c_MAX_HARDWARE_THREADS;
        const int minThreads = 0;
        const int maxThreads = NOUS_Multithreading::c_MAX_HARDWARE_THREADS * 2;

        if (ImGui::InputInt("Thread Count", &newSize, 1, 5))
        {
            newSize = std::clamp(newSize, minThreads, maxThreads);
        }

        ImGui::SameLine();
        if (ImGui::Button("Resize Pool"))
        {
            External->jobSystem->Resize(newSize);
        }
        
        ImGui::Columns(2);
        ImGui::Text("Max Hardware Threads: %u", NOUS_Multithreading::c_MAX_HARDWARE_THREADS);
        ImGui::NextColumn();

        static const auto& threadPool = External->jobSystem->GetThreadPool();
        static const auto& threads = threadPool.GetThreads();
        auto* mainThread = NOUS_Multithreading::GetMainThread();

        NOUS_Multithreading::NOUS_Job mainThreadJob("Nous Engine", {});
        mainThread->SetCurrentJob(&mainThreadJob);

        // Calculate active threads
        int activeThreads = 0;
        std::vector<NOUS_Multithreading::NOUS_Thread*> allThreads;
        if (mainThread) allThreads.push_back(mainThread);
        allThreads.insert(allThreads.end(), threads.begin(), threads.end());

        for (const auto& thread : allThreads)
            if (thread->GetThreadState() == NOUS_Multithreading::ThreadState::RUNNING)
                activeThreads++;

        // Active threads progress bar with threshold-based coloring
        float progress = static_cast<float>(activeThreads) / allThreads.size();
        const float yellowThreshold = 1.0f / 3.0f;
        const float redThreshold = 2.0f / 3.0f;

        // Determine color based on current progress
        ImVec4 barColor;
        if (progress <= yellowThreshold) {
            barColor = ImVec4(0.0f, 0.8f, 0.0f, 1.0f);  // Green
        }
        else if (progress <= redThreshold) {
            barColor = ImVec4(0.8f, 0.8f, 0.0f, 1.0f);  // Yellow
        }
        else {
            barColor = ImVec4(0.8f, 0.0f, 0.0f, 1.0f);  // Red
        }

        // Apply custom color to the progress bar
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, barColor);
        std::string text = "Active Threads: " + std::to_string(activeThreads) + "/" + std::to_string(allThreads.size());
        ImGui::ProgressBar(progress, ImVec2(-1, 0), text.c_str());
        ImGui::PopStyleColor();

        ImGui::Columns(1);

        ImGui::Separator();

        // Thread details table
        if (ImGui::BeginTable("ThreadsTable", 5,
            ImGuiTableFlags_Borders |
            ImGuiTableFlags_RowBg |
            ImGuiTableFlags_Resizable |
            ImGuiTableFlags_ScrollY))
        {
            // Table headers
            ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 100.0f);
            ImGui::TableSetupColumn("Current Job", ImGuiTableColumnFlags_WidthFixed, 100.0f);
            ImGui::TableSetupColumn("Time (s)", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableHeadersRow();

            // Table contents
            for (const auto& thread : allThreads)
            {
                ImGui::TableNextRow();

                // Status indicator
                const auto state = thread->GetThreadState();
                const ImVec4 color = (state == NOUS_Multithreading::ThreadState::RUNNING) ?
                    ImVec4(1.0f, 0.0f, 0.0f, 1.0f) :  // Green for running
                    ImVec4(0.0f, 1.0f, 0.0f, 1.0f);   // Red for others

                // Thread ID
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%lu", thread->GetID());

                // Name
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%s", thread->GetName().c_str());

                // State
                ImGui::TableSetColumnIndex(2);
                ImGui::TextColored(color, "%s",
                    NOUS_Multithreading::NOUS_Thread::GetStringFromState(state).c_str());

                // Job
                ImGui::TableSetColumnIndex(3);
                if (thread->GetCurrentJob()) 
                {
                    ImGui::Text("%s", thread->GetCurrentJob()->GetName().c_str());
                }
                else 
                {
                    ImGui::Text("%s", "None");
                }

                // Time
                ImGui::TableSetColumnIndex(4);
                ImGui::Text("%.3f", thread->GetExecutionTimeMS() / 1000);
            }

            ImGui::EndTable();
        }
    }
    ImGui::End();
}