#include "Editor/UI/Windows/JobQueueWindow/include/JobQueueWindow.h"

#include "Engine/NOUS_Multithreading/NOUS_ThreadPool/include/NOUS_ThreadPool.h"
#include "Engine/NOUS_Multithreading/NOUS_JobSystem/include/NOUS_JobSystem.h"
#include "Engine/NOUS_Multithreading/NOUS_Job/include/NOUS_Job.h"
#include "Engine/NOUS_Multithreading/NOUS_Multithreading.h"

#include "imgui.h"

#include <format>
#include <queue>

JobQueue::JobQueue(const char* title, EditorContext* context, bool start_open)
    : IEditorWindow(title, context, nullptr, start_open)
{
    Init();
}

void JobQueue::Init()
{

}

void JobQueue::Draw()
{
    if (!*p_open) return;

    if (ImGui::Begin(title, p_open))
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

            std::queue<nous::engine::multithreading::NOUS_Job*> tempQueue = std::move(jobQueue);

            while (!tempQueue.empty())
            {
                nous::engine::multithreading::NOUS_Job* job = tempQueue.front();
                tempQueue.pop();

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);

                if (job) {
                    ImGui::Text("%s", job->GetName().c_str());
                }
                else {
                    ImGui::TextDisabled("(null job)");
                }
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();
}