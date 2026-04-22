#include "Editor/UI/Windows/MultithreadingWindow/include/MultithreadingWindow.h"

#include "Engine/NOUS_Multithreading/NOUS_JobSystem/include/NOUS_JobSystem.h"
#include "Engine/NOUS_Multithreading/NOUS_Job/include/NOUS_Job.h"
#include "Engine/NOUS_Multithreading/NOUS_Thread/include/NOUS_Thread.h"
#include "Engine/NOUS_Multithreading/NOUS_Multithreading.h"
#include "Engine/Renderer/Backend/Vulkan/Rendering/CommandBuffer/VulkanMultithreading.h"
#include "Engine/Renderer/Backend/Vulkan/VulkanBackend.h"
#include "Engine/NOUS_Multithreading/NOUS_ThreadPool/include/NOUS_ThreadPool.h"

#include <algorithm>
#include <format>

#include "imgui.h"

Multithreading::Multithreading(const char* title, EditorContext* context, bool start_open)
    : IEditorWindow(title, context, nullptr, start_open)
{
}

void Multithreading::Update()
{
    ImGui::SetNextWindowSize(ImVec2(650, 400), ImGuiCond_FirstUseEver);
}

void Multithreading::DrawContent()
{
    // System overview section
    ImGui::Text("Job System Overview");
    ImGui::Separator();

    static int newSize = nous::engine::multithreading::c_MAX_HARDWARE_THREADS;
    const int minThreads = 0;
    const int maxThreads = nous::engine::multithreading::c_MAX_HARDWARE_THREADS * 2;

    if (ImGui::InputInt("Thread Count", &newSize, 1, 5))
    {
        newSize = std::clamp(newSize, minThreads, maxThreads);
    }

    ImGui::SameLine();
    if (ImGui::Button("Resize Pool"))
    {
        editorContext->GetJobSystem()->Resize(static_cast<uint8_t>(newSize));
        NOUS_VulkanMultithreading::RecreateWorkerCommandPools(VulkanBackend::GetVulkanContext());
    }

    ImGui::Separator();

    const auto& threadPool = editorContext->GetJobSystem()->GetThreadPool();
    const auto& threads = threadPool.GetThreads();
    auto jobQueue = threadPool.GetJobQueueSnapshot();

    ImGui::Columns(2);
    ImGui::Text("Max Hardware Threads: %u", nous::engine::multithreading::c_MAX_HARDWARE_THREADS);
    ImGui::Text("Total Worker Threads: %u", static_cast<uint8>(threads.size()));
    ImGui::Text("Total Jobs: %u", editorContext->GetJobSystem()->GetPendingJobs());
    ImGui::NextColumn();

    auto* mainThread = nous::engine::multithreading::GetMainThread();

    // Static lifetime — avoids a dangling pointer when Draw() returns.
    static nous::engine::multithreading::NOUS_Job mainThreadJob("Nous Engine", {});
    mainThread->SetCurrentJob(&mainThreadJob);

    // Calculate active threads
    int activeThreads = 0;
    std::vector<nous::engine::multithreading::NOUS_Thread*> allThreads;
    if (mainThread) allThreads.push_back(mainThread);
    allThreads.insert(allThreads.end(), threads.begin(), threads.end());

    for (const auto& thread : allThreads)
        if (thread->GetThreadState() == nous::engine::multithreading::ThreadState::RUNNING)
            activeThreads++;

    // Active threads progress bar with threshold-based coloring
    float progress = static_cast<float>(activeThreads) / static_cast<float>(allThreads.size());
    const float yellowThreshold = 1.0f / 3.0f;
    const float redThreshold = 2.0f / 3.0f;

    // Determine color based on current progress
    ImVec4 barColor;
    if (progress <= yellowThreshold)
    {
        barColor = ImVec4(0.0f, 0.8f, 0.0f, 1.0f); // Green
    }
    else if (progress <= redThreshold)
    {
        barColor = ImVec4(0.8f, 0.8f, 0.0f, 1.0f); // Yellow
    }
    else
    {
        barColor = ImVec4(0.8f, 0.0f, 0.0f, 1.0f); // Red
    }

    // Apply custom color to the progress bar
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, barColor);
    std::string text = std::format("Active Threads: {}/{}", activeThreads, allThreads.size());
    ImGui::ProgressBar(progress, ImVec2(-1, 0), text.c_str());
    ImGui::PopStyleColor();

    // Determine and display threading mode
    const bool isSingleThreaded = threads.empty();
    const char* modeText = isSingleThreaded ? "Single-threaded Mode" : "Multi-threaded Mode";
    const ImVec4 modeColor = isSingleThreaded ? ImVec4(0.8f, 0.0f, 0.0f, 1.0f) : ImVec4(0.1f, 0.6f, 1.0f, 1.0f);

    // Create centered container
    ImGui::BeginChild("##ModeTextContainer", ImVec2(-1, 30), true);
    {
        ImVec2 textSize = ImGui::CalcTextSize(modeText);
        ImGui::SetCursorPos(ImVec2(
            (ImGui::GetWindowWidth() - textSize.x) * 0.5f,
            (ImGui::GetWindowHeight() - textSize.y) * 0.5f
        ));
        ImGui::TextColored(modeColor, "%s", modeText);
    }
    ImGui::EndChild();

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
            const ImVec4 color = (state == nous::engine::multithreading::ThreadState::RUNNING)
                                     ? ImVec4(1.0f, 0.0f, 0.0f, 1.0f)
                                     : // Green for running
                                     ImVec4(0.0f, 1.0f, 0.0f, 1.0f); // Red for others

            // Thread ID
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%u", nous::engine::multithreading::NOUS_Thread::GetDisplayID(thread->GetID()));

            // Name
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", thread->GetName().c_str());

            // State
            ImGui::TableSetColumnIndex(2);
            ImGui::TextColored(color, "%s",
                               nous::engine::multithreading::NOUS_Thread::GetStringFromState(state).c_str());

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
            ImGui::Text("%.3f", thread->GetExecutionTimeMS() / 1000.0f);
        }

        ImGui::EndTable();
    }
}
