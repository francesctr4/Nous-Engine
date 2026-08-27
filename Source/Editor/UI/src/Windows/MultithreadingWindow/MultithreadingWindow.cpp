#include <EditorUI/MultithreadingWindow.h>

#include <NOUS_Multithreading/NOUS_JobSystem.h>
#include <NOUS_Multithreading/NOUS_Job.h>
#include <NOUS_Multithreading/NOUS_Thread.h>
#include <NOUS_Multithreading/NOUS_Multithreading.h>
#include <Renderer/iEditorRenderBridge.h>
#include <NOUS_Multithreading/NOUS_ThreadPool.h>

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

        // The per-worker command pools are sized to the thread count, so they must
        // be rebuilt whenever the pool is resized.
        if (IEditorRenderBridge* bridge = editorContext->GetEditorRenderBridge())
            bridge->RecreateWorkerCommandPools();
    }

    ImGui::Separator();

    const auto& threadPool = editorContext->GetJobSystem()->GetThreadPool();
    const auto& threads = threadPool.GetThreads();

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
            // Read the name snapshot, never the job pointer: the worker frees the
            // job as soon as it finishes, so dereferencing it from here was a
            // use-after-free (and the old two-call null-check could also see a
            // non-null pointer go null between the check and the dereference).
            const std::string jobName = thread->GetCurrentJobName();
            ImGui::Text("%s", jobName.empty() ? "None" : jobName.c_str());

            // Time
            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%.3f", thread->GetExecutionTimeMS() / 1000.0f);
        }

        ImGui::EndTable();
    }
}
