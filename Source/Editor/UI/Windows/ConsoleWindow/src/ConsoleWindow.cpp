#include "Editor/UI/Windows/ConsoleWindow/include/ConsoleWindow.h"
#include <algorithm>
#include "Engine/Core/TimeManager/TimeManager.h"
#include "imgui.h"

#include <mutex>
static std::mutex consoleMutex;

// Colors for different log levels
ImVec4 levelColors[6] = {
        ImVec4(1.0f, 0.0f, 0.0f, 1.0f),        // FATAL - Red
        ImVec4(1.0f, 0.4f, 0.4f, 1.0f),        // ERROR - Light Red
        ImVec4(1.0f, 1.0f, 0.0f, 1.0f),        // WARN - Yellow
        ImVec4(0.0f, 1.0f, 0.0f, 1.0f),        // INFO - Green
        ImVec4(0.0f, 0.5f, 1.0f, 1.0f),        // DEBUG - Blue
        ImVec4(0.5f, 0.5f, 0.5f, 1.0f)         // TRACE - Gray
};

ConsoleWindow::ConsoleWindow(const char* title, EditorContext* context, bool start_open)
        : IEditorWindow(title, context, nullptr, start_open)
{
    for (bool & i : showChannel)
        i = true;

    Init();
}

ConsoleWindow::~ConsoleWindow()
{
    SetLogCallback(nullptr);
}

void ConsoleWindow::Init()
{
    // 🔧 updated for tuple-based history
    const auto& history = GetLogHistory();
    logBuffer.clear();
    for (const auto& [level, channel, time, msg] : history)
        logBuffer.emplace_back(level, channel, time, msg);

    SetLogCallback([this](LogLevel level, LogChannel channel, double time, const char* message) {
        if (freezeConsole) return;

        std::scoped_lock lock(consoleMutex); // ✅ lock for multi-threaded logging
        logBuffer.emplace_back(level, channel, time, message);

        if (logBuffer.size() > 10000)
            logBuffer.pop_front();

        scrollToBottom = true;
    });
}

void ConsoleWindow::Draw()
{
    if (!*p_open) return;

    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(title, p_open, ImGuiWindowFlags_MenuBar))
    {
        DrawMenuBar();
        DrawLogPanel();
        DrawCommandLine();
    }
    ImGui::End();
}

void ConsoleWindow::DrawMenuBar()
{
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::Button("Clear")) {
            logBuffer.clear();
            ClearLogHistory();
        }

        ImGui::SameLine(0, 50);
        ImGui::Checkbox("Auto-scroll", &autoScroll);
        ImGui::SameLine();
        if (ImGui::Checkbox("Freeze", &freezeConsole))
            SetLoggingPaused(freezeConsole);

        ImGui::SameLine(0, 50);
        ImGui::Text("Levels:");
        ImGui::SameLine();

// Build summary text (selected levels)
        std::string selectedLevels;
        int activeLevels = 0;
        for (int i = 0; i < (int)LogLevel::LOG_LEVEL_MAX; ++i)
        {
            if (showLevel[i]) {
                if (!selectedLevels.empty()) selectedLevels += ", ";
                selectedLevels += levelNames[i];
                activeLevels++;
            }
        }

        if (activeLevels == (int)LogLevel::LOG_LEVEL_MAX)
            selectedLevels = "All Levels";
        else if (activeLevels == 0)
            selectedLevels = "No Level Selected";

        ImGui::SetNextItemWidth(240);
        if (ImGui::BeginCombo("##LevelFilter", selectedLevels.c_str()))
        {
            // “Select All” / “Deselect All” options
            if (ImGui::Selectable("Select All", false)) {
                for (int i = 0; i < (int)LogLevel::LOG_LEVEL_MAX; ++i) {
                    showLevel[i] = true;
                    SetLogLevelEnabled((LogLevel)i, true);
                }
            }

            if (ImGui::Selectable("Deselect All", false)) {
                for (int i = 0; i < (int)LogLevel::LOG_LEVEL_MAX; ++i) {
                    showLevel[i] = false;
                    SetLogLevelEnabled((LogLevel)i, false);
                }
            }

            ImGui::Separator();

            // Individual level toggles
            for (int i = 0; i < (int)LogLevel::LOG_LEVEL_MAX; ++i)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, levelColors[i]);
                if (ImGui::Selectable(levelNames[i], &showLevel[i], ImGuiSelectableFlags_DontClosePopups))
                    SetLogLevelEnabled((LogLevel)i, showLevel[i]);
                ImGui::PopStyleColor();
            }

            ImGui::EndCombo();
        }

        // 🧩 NEW: Channel filtering
        ImGui::SameLine(0, 50);
        ImGui::Text("Channels:");
        ImGui::SameLine();

// Collect which channels are actually present in the log
        std::vector<bool> channelUsed((int)LogChannel::MAX_CHANNELS, false);
        {
            std::scoped_lock lock(consoleMutex); // ✅ protect iteration
            for (const auto& [lvl, ch, time, msg] : logBuffer)
            {
                int idx = static_cast<int>(ch);
                if (idx >= 0 && idx < (int)LogChannel::MAX_CHANNELS)
                    channelUsed[idx] = true;
            }
        }

// Build a human-readable summary
        std::string selectedChannels;
        int activeCount = 0;
        for (int i = 0; i < (int)LogChannel::MAX_CHANNELS; ++i) {
            if (showChannel[i]) {
                if (!selectedChannels.empty()) selectedChannels += ", ";
                selectedChannels += LOG_CHANNEL_NAMES[i];
                activeCount++;
            }
        }
        if (activeCount == (int)LogChannel::MAX_CHANNELS)
            selectedChannels = "All Channels";
        else if (activeCount == 0)
            selectedChannels = "No Channel Selected";

// Dropdown combo
        ImGui::SetNextItemWidth(360);
        if (ImGui::BeginCombo("##ChannelFilter", selectedChannels.c_str(), ImGuiComboFlags_HeightLargest)) // combo label
        {
            // Optional: Select/Deselect All
            if (ImGui::Selectable("Select All", false)) {
                for (int i = 0; i < (int)LogChannel::MAX_CHANNELS; ++i)
                    if (channelUsed[i]) showChannel[i] = true;
            }
            if (ImGui::Selectable("Deselect All", false)) {
                for (bool & i : showChannel)
                    i = false;
            }

            ImGui::Separator();

            // Show only channels that actually appeared in logs
            for (int i = 0; i < (int)LogChannel::MAX_CHANNELS; ++i) {
                if (!channelUsed[i])
                    continue; // skip unused channels
                ImGui::Selectable(LOG_CHANNEL_NAMES[i], &showChannel[i], ImGuiSelectableFlags_DontClosePopups);
            }

            ImGui::EndCombo();
        }

        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 350);
        ImGui::PushItemWidth(250);
        ImGui::InputTextWithHint("##Search", "Search logs...", searchBuffer, IM_ARRAYSIZE(searchBuffer));
        ImGui::PopItemWidth();

        ImGui::SameLine();
        ImGui::TextDisabled("%zu logs", logBuffer.size());

        ImGui::EndMenuBar();
    }
}

void ConsoleWindow::DrawLogPanel()
{
    // Snapshot copy (type-safe without knowing element type)
    decltype(logBuffer) snapshot;
    {
        std::scoped_lock lock(logMutex);
        snapshot = logBuffer;
    }

    const float footer_height =
        ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();

    ImGui::BeginChild("ScrollingRegion",
        ImVec2(0, -footer_height),
        false,
        ImGuiWindowFlags_HorizontalScrollbar);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));

    const bool hasSearch = searchBuffer[0] != '\0';
    std::string searchLower;

    if (hasSearch)
    {
        searchLower = searchBuffer;
        std::transform(
            searchLower.begin(),
            searchLower.end(),
            searchLower.begin(),
            [](unsigned char c) { return (char)std::tolower(c); });
    }

    // Iterate over snapshot (safe from concurrent modification)
    for (const auto& [level, channel, time, text] : snapshot)
    {
        if (!showLevel[(int)level]) continue;
        if (!showChannel[(int)channel]) continue;

        // Case-insensitive search
        if (hasSearch)
        {
            std::string textLower = text;
            std::transform(
                textLower.begin(),
                textLower.end(),
                textLower.begin(),
                [](unsigned char c) { return (char)std::tolower(c); });

            if (textLower.find(searchLower) == std::string::npos)
                continue;
        }

        // Convert seconds (double) → MM:SS:MMM
        int totalMs = static_cast<int>(time * 1000.0);
        int minutes = (totalMs / 1000) / 60;
        int seconds = (totalMs / 1000) % 60;
        int millis  = totalMs % 1000;

        char timeBuffer[16];
        snprintf(timeBuffer, sizeof(timeBuffer),
                 "%02d:%02d:%03d",
                 minutes, seconds, millis);

        ImGui::PushStyleColor(ImGuiCol_Text,
                              levelColors[(int)level]);

        ImGui::Text("[%s] [%s] %s",
                    timeBuffer,
                    LOG_CHANNEL_NAMES[(int)channel],
                    text.c_str());

        ImGui::PopStyleColor();
    }

    if (scrollToBottom &&
        (autoScroll || ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
    {
        ImGui::SetScrollHereY(1.0f);
        scrollToBottom = false;
    }

    ImGui::PopStyleVar();
    ImGui::EndChild();
}

void ConsoleWindow::DrawCommandLine()
{
    ImGui::Separator();

    bool reclaimFocus = false;
    ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_EnterReturnsTrue;

    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 150);
    if (ImGui::InputText("##Input", inputBuffer, IM_ARRAYSIZE(inputBuffer), inputFlags))
    {
        char* input = inputBuffer;
        char* start = input;
        while (*start && (*start == ' ' || *start == '\t')) start++;
        char* end = start + strlen(start) - 1;
        while (end > start && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) end--;
        *(end + 1) = 0;

        if (start[0]) {
            ExecuteCommand(start);
        }
        strcpy_s(inputBuffer, "");
        reclaimFocus = true;
    }
    ImGui::PopItemWidth();

    ImGui::SameLine();
    ImGui::TextDisabled("(type 'help' for commands)");

    ImGui::SetItemDefaultFocus();
    if (reclaimFocus)
        ImGui::SetKeyboardFocusHere(-1);
}

void ConsoleWindow::ExecuteCommand(const std::string& command)
{
    commandHistory.push_back(command);
    if (commandHistory.size() > 100)
        commandHistory.erase(commandHistory.begin());
    historyPos = -1;

    LogOutput(LOG_LEVEL_INFO, "> %s", command.c_str());

    if (command == "clear" || command == "cls") {
        logBuffer.clear();
        ClearLogHistory();
    }
    else if (command == "help") {
        LogOutput(LOG_LEVEL_INFO, "Available commands:");
        LogOutput(LOG_LEVEL_INFO, "  clear, cls - Clear the console");
        LogOutput(LOG_LEVEL_INFO, "  help - Show this help message");
        LogOutput(LOG_LEVEL_INFO, "  log_test - Test all log levels");
    }
    else if (command == "log_test") {
        LogOutput(LOG_LEVEL_FATAL, "This is a fatal message");
        LogOutput(LOG_LEVEL_ERROR, "This is an error message");
        LogOutput(LOG_LEVEL_WARN,  "This is a warning message");
        LogOutput(LOG_LEVEL_INFO,  "This is an info message");
        LogOutput(LOG_LEVEL_DEBUG, "This is a debug message");
        LogOutput(LOG_LEVEL_TRACE, "This is a trace message");
    }
    else {
        LogOutput(LOG_LEVEL_ERROR, "Unknown command: '%s'", command.c_str());
        LogOutput(LOG_LEVEL_INFO, "Type 'help' for available commands");
    }
}