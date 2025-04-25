#include "JobQueueWindow.h"

#include "Application.h"
#include "NOUS_JobSystem.h"
#include "NOUS_Multithreading.h"
#include "VulkanMultithreading.h"
#include "VulkanBackend.h"

#include <algorithm>

JobQueue::JobQueue(const char* title, bool start_open)
    : IEditorWindow(title, nullptr, start_open)
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
        const auto& threadPool = External->jobSystem->GetThreadPool();
        const auto& jobQueue = threadPool.GetJobQueue();

        // New Job Queue table
        if (ImGui::BeginTable("JobQueue", 1,
            ImGuiTableFlags_Borders |
            ImGuiTableFlags_RowBg |
            ImGuiTableFlags_ScrollY))
        {
            ImGui::TableSetupColumn(std::format("Job Name ({} pending jobs)", jobQueue.size()).c_str(), ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            // Create a copy of the queue for safe iteration
            std::queue<NOUS_Multithreading::NOUS_Job*> tempQueue = jobQueue;

            while (!tempQueue.empty())
            {
                NOUS_Multithreading::NOUS_Job* job = tempQueue.front();
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