#include "Editor/Windows/ConsoleWindow.h"
#include <algorithm>

#include <imgui.h>

// Colors for different log levels
ImVec4 levelColors[6] = {
        ImVec4(1.0f, 0.0f, 0.0f, 1.0f),        // FATAL - Red
        ImVec4(1.0f, 0.4f, 0.4f, 1.0f),        // ERROR - Light Red
        ImVec4(1.0f, 1.0f, 0.0f, 1.0f),        // WARN - Yellow
        ImVec4(0.0f, 1.0f, 0.0f, 1.0f),        // INFO - Green
        ImVec4(0.0f, 0.5f, 1.0f, 1.0f),        // DEBUG - Blue
        ImVec4(0.5f, 0.5f, 0.5f, 1.0f)         // TRACE - Gray
};

ConsoleWindow::ConsoleWindow(const char* title, bool start_open)
        : IEditorWindow(title, nullptr, start_open)
{
    Init();
}

ConsoleWindow::~ConsoleWindow()
{
    // Unregister the callback when the window is destroyed
    SetLogCallback(nullptr);
}

void ConsoleWindow::Init()
{
    // Initialize the log buffer with existing logs
    const auto& history = GetLogHistory();
    logBuffer.assign(history.begin(), history.end());

    // Register callback to receive new logs in real-time
    SetLogCallback([this](LogLevel level, const char* message) {
        if (freezeConsole)
            return;

        logBuffer.emplace_back(level, message);

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
        // 🧹 Clear logs button
        if (ImGui::Button("Clear")) {
            logBuffer.clear();
            ClearLogHistory();
        }

        // Add some spacing between groups
        ImGui::SameLine(0, 50);

        // ⚙️ Behavior toggles
        ImGui::Checkbox("Auto-scroll", &autoScroll);
        ImGui::SameLine();
        if (ImGui::Checkbox("Freeze", &freezeConsole)) {
            SetLoggingPaused(freezeConsole);
        }

        ImGui::SameLine(0, 50);

        // 🧩 Channels
        ImGui::Text("Channels:");
        ImGui::SameLine();

        for (int i = 0; i < 6; i++) {
            if (i > 0) ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, levelColors[i]);
            if (ImGui::Checkbox(levelNames[i], &showLevel[i])) {
                SetLogLevelEnabled((LogLevel)i, showLevel[i]);
            }
            ImGui::PopStyleColor();
        }

        // 🔍 Search bar aligned to the right
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 350);
        ImGui::PushItemWidth(250);
        ImGui::InputTextWithHint("##Search", "Search logs...", searchBuffer, IM_ARRAYSIZE(searchBuffer));
        ImGui::PopItemWidth();

        // 🧾 Log count
        ImGui::SameLine();
        ImGui::TextDisabled("%zu logs", logBuffer.size());

        ImGui::EndMenuBar();
    }
}

void ConsoleWindow::DrawLogPanel()
{
    const float footer_height = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height), false,
                      ImGuiWindowFlags_HorizontalScrollbar);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));

    const bool hasSearch = searchBuffer[0] != '\0';
    std::string searchLower;
    if (hasSearch)
    {
        searchLower = searchBuffer;
        std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);
    }

    for (const auto& log : logBuffer)
    {
        if (!showLevel[log.first])
            continue;

        const std::string& logText = log.second;

        // Case-insensitive search
        if (hasSearch)
        {
            std::string logLower = logText;
            std::transform(logLower.begin(), logLower.end(), logLower.begin(), ::tolower);

            if (logLower.find(searchLower) == std::string::npos)
                continue;
        }

        // Display log normally
        ImGui::PushStyleColor(ImGuiCol_Text, levelColors[log.first]);
        ImGui::TextUnformatted(logText.c_str());
        ImGui::PopStyleColor();
    }

    // Auto-scroll
    if (scrollToBottom && (autoScroll || ImGui::GetScrollY() >= ImGui::GetScrollMaxY())) {
        ImGui::SetScrollHereY(1.0f);
        scrollToBottom = false;
    }

    ImGui::PopStyleVar();
    ImGui::EndChild();
}

void ConsoleWindow::DrawCommandLine()
{
    ImGui::Separator();

    // Command-line input
    bool reclaimFocus = false;
    ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_EnterReturnsTrue;

    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 150);
    if (ImGui::InputText("##Input", inputBuffer, IM_ARRAYSIZE(inputBuffer), inputFlags))
    {
        char* input = inputBuffer;
        // Trim whitespace
        char* start = input;
        while (*start && (*start == ' ' || *start == '\t')) start++;
        char* end = start + strlen(start) - 1;
        while (end > start && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) end--;
        *(end + 1) = 0;

        if (start[0]) {
            ExecuteCommand(start);
        }
        strcpy(inputBuffer, "");
        reclaimFocus = true;
    }
    ImGui::PopItemWidth();

    // Add a help hint
    ImGui::SameLine();
    ImGui::TextDisabled("(type 'help' for commands)");

    // Auto-focus on window apparition
    ImGui::SetItemDefaultFocus();
    if (reclaimFocus) {
        ImGui::SetKeyboardFocusHere(-1); // Auto focus previous widget
    }
}

void ConsoleWindow::ExecuteCommand(const std::string& command)
{
    // Add command to history
    commandHistory.push_back(command);
    if (commandHistory.size() > 100) {
        commandHistory.erase(commandHistory.begin());
    }
    historyPos = -1;

    // Log the command
    LogOutput(LOG_LEVEL_INFO, "> %s", command.c_str());

    // Process commands
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
        LogOutput(LOG_LEVEL_WARN, "This is a warning message");
        LogOutput(LOG_LEVEL_INFO, "This is an info message");
        LogOutput(LOG_LEVEL_DEBUG, "This is a debug message");
        LogOutput(LOG_LEVEL_TRACE, "This is a trace message");
    }
    else {
        LogOutput(LOG_LEVEL_ERROR, "Unknown command: '%s'", command.c_str());
        LogOutput(LOG_LEVEL_INFO, "Type 'help' for available commands");
    }
}