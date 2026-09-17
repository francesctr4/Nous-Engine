#include <EditorUI/JobQueueWindow.h>

#include <NOUS_Multithreading/NOUS_ThreadPool.h>
#include <NOUS_Multithreading/NOUS_JobSystem.h>
#include <NOUS_Multithreading/NOUS_Thread.h>

#include "imgui.h"

#include <chrono>
#include <cmath>
#include <format>
#include <string>

JobQueue::JobQueue(const char* title, EditorContext* context, const bool start_open)
    : IEditorWindow(title, context, nullptr, start_open)
{
}

void JobQueue::DrawQueue() const
{
    const auto& threadPool = editorContext->GetJobSystem()->GetThreadPool();
    auto jobQueue = threadPool.GetJobQueueSnapshot();

    // A snapshot of NAMES, by value. The pool deletes a job the instant it
    // completes, so holding the NOUS_Job pointers would race the free -- see the
    // Editor-Observes-Engine rule.
    if (ImGui::BeginTable("JobQueue", 1,
        ImGuiTableFlags_Borders |
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_ScrollY))
    {
        ImGui::TableSetupColumn(std::format("Job Name ({} pending jobs)", jobQueue.size()).c_str(),
                                ImGuiTableColumnFlags_WidthStretch);
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

void JobQueue::DrawTests() const
{
    // Synthetic load, which nothing else in the editor can produce -- and the only
    // way to make this window and the Multithreading window show anything at all,
    // since real engine work drains faster than it can be watched.
    //
    // These were F7 and F8 in Application's HandleDebugKeys, reachable from anywhere
    // in the editor and discoverable from nowhere. They belong beside the readout
    // they exist to drive.
    nous::engine::multithreading::NOUS_JobSystem* jobSystem = editorContext->GetJobSystem();
    if (!jobSystem)
    {
        ImGui::TextDisabled("No job system.");
        return;
    }

    ImGui::TextWrapped("Submits synthetic jobs so the queue above, and the worker "
                       "states in the Multithreading window, have something to show.");
    ImGui::Spacing();

    // One long job: occupies a worker while the queue drains behind it, which is
    // what makes the difference between "running" and "queued" visible.
    if (ImGui::Button("Sleep 5s (1 job)", ImVec2(180.0f, 0.0f)))
    {
        jobSystem->SubmitJob([]
        {
            nous::engine::multithreading::NOUS_Thread::SleepMS(5000);
        }, "Test Sleep");
    }

    ImGui::SameLine();
    ImGui::TextDisabled("occupies one worker");

    // Many short ones: submitted faster than the pool drains them, which is the only
    // way the pending count above climbs above zero for long enough to read.
    if (ImGui::Button("Stress (100 jobs)", ImVec2(180.0f, 0.0f)))
    {
        for (int i = 0; i < 100; ++i)
        {
            jobSystem->SubmitJob([]
            {
                // A BUSY loop, not a sleep: a sleeping worker is idle as far as the
                // OS is concerned, so it would not show the pool saturated.
                constexpr std::chrono::milliseconds duration(500);
                const auto start = std::chrono::steady_clock::now();
                while (std::chrono::steady_clock::now() - start < duration)
                    (void)std::sqrt(123.456);
            }, "Stress Test");
        }
    }

    ImGui::SameLine();
    ImGui::TextDisabled("fills the queue faster than it drains");
}

void JobQueue::DrawContent()
{
    if (!ImGui::BeginTabBar("##jobQueueTabs"))
        return;

    if (ImGui::BeginTabItem("Queue"))
    {
        DrawQueue();
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Tests"))
    {
        DrawTests();
        ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
}
