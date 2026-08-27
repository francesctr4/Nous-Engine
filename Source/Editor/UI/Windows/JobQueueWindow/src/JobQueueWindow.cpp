#include "Editor/UI/Windows/JobQueueWindow/include/JobQueueWindow.h"

#include <NOUS_Multithreading/NOUS_ThreadPool.h>
#include <NOUS_Multithreading/NOUS_JobSystem.h>

#include "imgui.h"

#include <format>
#include <string>

JobQueue::JobQueue(const char* title, EditorContext* context, const bool start_open)
    : IEditorWindow(title, context, nullptr, start_open)
{
}

void JobQueue::DrawContent()
{
    const auto& threadPool = editorContext->GetJobSystem()->GetThreadPool();
    auto jobQueue = threadPool.GetJobQueueSnapshot();

    // New Job Queue table
    if (ImGui::BeginTable("JobQueue", 1,
        ImGuiTableFlags_Borders |
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_ScrollY))
    {
        ImGui::TableSetupColumn(std::format("Job Name ({} pending jobs)", jobQueue.size()).c_str(), ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (const std::string& jobName : jobQueue)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", jobName.c_str());
        }
        ImGui::EndTable();
    }
}