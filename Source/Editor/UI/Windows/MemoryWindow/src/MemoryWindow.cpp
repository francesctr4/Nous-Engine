#include "Editor/UI/Windows/MemoryWindow/include/MemoryWindow.h"
#include <MemoryManager/MemoryManager.h>

#include <imgui.h>
#include <algorithm>
#include <cinttypes>
#include <numeric>
#include <utility>

MemoryWindow::MemoryWindow(const char* title, EditorContext* context, bool start_open)
    : IEditorWindow(title, context, nullptr, start_open)
{
}

void MemoryWindow::Update()
{
    ImGui::SetNextWindowSize(ImVec2(650, 400), ImGuiCond_FirstUseEver);
}

void MemoryWindow::DrawContent()
{
    auto stats = nous::engine::memory::GetMemoryStats();
    auto config = nous::engine::memory::GetMemoryConfig();
    const char* const* tags = nous::engine::memory::GetMemoryTagNames();

    // ------------------------------
    // Compute metrics
    // ------------------------------
    float usagePercentage = 0.0f;
    if (config.totalAllocationSize > 0)
        usagePercentage = static_cast<float>(stats.totalAllocated) /
            static_cast<float>(config.totalAllocationSize);

    history.AddValue(usagePercentage * 100.0f); // store percent

    float totalPoolMB = static_cast<float>(config.totalAllocationSize) / (1024.0f * 1024.0f);
    float totalAllocatedMB = static_cast<float>(stats.totalAllocated) / (1024.0f * 1024.0f);

    // ------------------------------
    // Summary info
    // ------------------------------
    ImGui::SeparatorText("Memory Summary");

    if (ImGui::BeginTable("MemSummary", 2,
                          ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoBordersInBody))
    {
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthStretch, 0.6f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.4f);

        auto Row = [](const char* label, const char* fmt, auto... args)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(label);
            ImGui::TableSetColumnIndex(1);
            ImGui::Text(fmt, args...);
        };

        Row("Total Pool Size", "%.2f MB", totalPoolMB);
        Row("Current Allocated Size", "%.2f MB", totalAllocatedMB);
        Row("Current Allocations", "%llu", stats.totalAllocations);
        Row("Usage", "%.2f%%", usagePercentage * 100.0f);

        ImGui::EndTable();
    }

    // ------------------------------
    // Color-coded progress bar
    // ------------------------------
    ImVec4 color;
    if (usagePercentage < 0.7f) color = ImVec4(0.2f, 0.8f, 0.2f, 1.0f); // green
    else if (usagePercentage < 0.9f) color = ImVec4(0.9f, 0.7f, 0.2f, 1.0f); // yellow
    else color = ImVec4(0.9f, 0.2f, 0.2f, 1.0f); // red

    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
    ImGui::ProgressBar(usagePercentage, ImVec2(-1, 0), "");
    ImGui::PopStyleColor();

    // ------------------------------
    // Real-time line plot
    // ------------------------------
    ImGui::SeparatorText("Usage over Time");
    ImGui::PlotLines(
        "##MemoryUsagePlot",
        history.values.data(),
        static_cast<int>(history.values.size()),
        history.currentIndex,
        nullptr,
        0.0f, 100.0f, // Y range (0–100%)
        ImVec2(-1, 100.0f) // full width, fixed height
    );

    // Optional stats
    float maxUsage = *std::ranges::max_element(history.values);
    ImGui::Text("Peak: %.2f%%", maxUsage);

    // ------------------------------
    // Per-tag breakdown
    // ------------------------------
    ImGui::SeparatorText("Per-Tag Breakdown");

    if (ImGui::BeginTable("MemoryTags", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn("Tag");
        ImGui::TableSetupColumn("Allocated (bytes)");
        ImGui::TableSetupColumn("Allocated (MB)");
        ImGui::TableHeadersRow();

        uint64 totalBytes = 0;

        for (uint32 i = 0; i < static_cast<uint32>(std::to_underlying(MemoryTag::MAX)); ++i)
        {
            uint64 bytes = stats.taggedAllocations[i];
            if (bytes == 0) continue;

            totalBytes += bytes;

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(tags[i]);
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%" PRIu64, bytes);
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.3f", static_cast<float>(bytes) / (1024.0f * 1024.0f));
        }

        // ------------------------------
        // Add a separator row before total
        // ------------------------------
        ImGui::TableNextRow();
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(ImVec4(0.2f, 0.2f, 0.2f, 1.0f)));

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.3f, 1.0f), "TOTAL");

        ImGui::TableSetColumnIndex(1);
        ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.3f, 1.0f), "%" PRIu64, totalBytes);

        ImGui::TableSetColumnIndex(2);
        ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.3f, 1.0f), "%.3f",
                           static_cast<float>(totalBytes) / (1024.0f * 1024.0f));

        ImGui::EndTable();
    }
}
