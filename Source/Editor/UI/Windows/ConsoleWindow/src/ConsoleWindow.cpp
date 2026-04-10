#include "Editor/UI/Windows/ConsoleWindow/include/ConsoleWindow.h"
#include "Engine/Core/TimeManager/TimeManager.h"
#include "imgui.h"

#include <algorithm>

// Returns just the filename portion of a full path (no allocation, no runtime search needed
// since the path was already resolved to just the filename at compile time via NOUS_SOURCE_FILE).
static const char* GetFileName(const char* path)
{
    if (!path) return "";
    const char* name = path;
    for (const char* p = path; *p; ++p)
        if (*p == '/' || *p == '\\') name = p + 1;
    return name;
}

static constexpr ImVec4 k_LevelColors[] = {
    ImVec4(1.0f, 0.0f, 0.0f, 1.0f),   // FATAL  — red
    ImVec4(1.0f, 0.4f, 0.4f, 1.0f),   // ERROR  — light red
    ImVec4(1.0f, 1.0f, 0.0f, 1.0f),   // WARN   — yellow
    ImVec4(0.0f, 1.0f, 0.0f, 1.0f),   // INFO   — green
    ImVec4(0.0f, 0.5f, 1.0f, 1.0f),   // DEBUG  — blue
    ImVec4(0.5f, 0.5f, 0.5f, 1.0f),   // TRACE  — grey
};

// ──────────────────────────────────────────────────────────────────────────────
// Construction
// ──────────────────────────────────────────────────────────────────────────────

ConsoleWindow::ConsoleWindow(const char* title, EditorContext* context, const bool start_open)
    : IEditorWindow(title, context, nullptr, start_open)
{
    for (bool& b : showChannel) b = true;
    ConsoleWindow::Init();
}

void ConsoleWindow::Init()
{
    // Seed the display buffer with whatever the Logger already holds.
    // GetLogEntriesSince returns the new cursor; storing it means next pull
    // only fetches entries that arrive after this point.
    logBuffer.clear();
    m_readCursor = GetLogEntriesSince(0, logBuffer);

    // Populate channelUsed from seeded entries.
    for (const auto& e : logBuffer) {
        if (const int ch = static_cast<int>(e.channel); ch >= 0 && ch < static_cast<int>(LogChannel::MAX_CHANNELS))
            m_channelUsed[ch] = true;
    }

    m_filterDirty     = true;
    m_lastCheckedSize = 0;
}

// ──────────────────────────────────────────────────────────────────────────────
// Pull — called once per frame, no mutex needed (render thread only)
// ──────────────────────────────────────────────────────────────────────────────

void ConsoleWindow::PullNewEntries()
{
    if (freezeConsole) return;

    const size_t prevSize = logBuffer.size();
    m_readCursor = GetLogEntriesSince(m_readCursor, logBuffer);

    if (logBuffer.size() == prevSize) return; // nothing new

    // Update channel presence incrementally (no O(n) full scan).
    for (size_t i = prevSize; i < logBuffer.size(); ++i)
    {
        if (const int ch = static_cast<int>(logBuffer[i].channel);
            ch >= 0 && ch < static_cast<int>(LogChannel::MAX_CHANNELS) && !m_channelUsed[ch])
        {
            m_channelUsed[ch]     = true;
            m_channelSummaryDirty = true;
        }
    }

    // Cap the display buffer to avoid unbounded growth across a long session.
    if (logBuffer.size() > k_MaxDisplayEntries) {
        const size_t excess = logBuffer.size() - k_MaxDisplayEntries;
        logBuffer.erase(logBuffer.begin(), logBuffer.begin() + static_cast<ptrdiff_t>(excess));

        // All stored indices are stale — force a full rebuild.
        m_filterDirty     = true;
        m_lastCheckedSize = 0;

        // Recompute channelUsed from scratch (rare path).
        memset(m_channelUsed, 0, sizeof(m_channelUsed));
        for (const auto& e : logBuffer) {
            if (const int ch = static_cast<int>(e.channel); ch >= 0 && ch < static_cast<int>(LogChannel::MAX_CHANNELS))
                m_channelUsed[ch] = true;
        }
        m_channelSummaryDirty = true;
    }

    scrollToBottom = true;
}

// ──────────────────────────────────────────────────────────────────────────────
// Filter helpers
// ──────────────────────────────────────────────────────────────────────────────

bool ConsoleWindow::PassesFilters(const LogEntry& entry) const
{
    if (!showLevel  [static_cast<int>(entry.level)  ]) return false;
    if (!showChannel[static_cast<int>(entry.channel)]) return false;

    if (!m_searchLower.empty()) {
        // Case-insensitive search with no heap allocation:
        // needle (m_searchLower) is already lowercased; tolower each haystack char inline.
        const auto it = std::ranges::search(entry.message, m_searchLower,
            [](const unsigned char a, const unsigned char b) { return std::tolower(a) == b; }).begin();

        if (it == entry.message.end()) return false;
    }

    return true;
}

void ConsoleWindow::RebuildFilteredIndices()
{
    m_filteredIndices.clear();
    m_filteredIndices.reserve(logBuffer.size());

    for (int i = 0; i < static_cast<int>(logBuffer.size()); ++i) {
        if (PassesFilters(logBuffer[i]))
            m_filteredIndices.push_back(i);
    }

    m_filterDirty     = false;
    m_lastCheckedSize = logBuffer.size();
}

void ConsoleWindow::UpdateFilteredIndicesIncremental(const size_t fromIndex)
{
    for (size_t i = fromIndex; i < logBuffer.size(); ++i) {
        if (PassesFilters(logBuffer[i]))
            m_filteredIndices.push_back(static_cast<int>(i));
    }
    m_lastCheckedSize = logBuffer.size();
}

// ──────────────────────────────────────────────────────────────────────────────
// Clear
// ──────────────────────────────────────────────────────────────────────────────

void ConsoleWindow::ClearDisplay()
{
    logBuffer.clear();
    m_filteredIndices.clear();
    memset(m_channelUsed, 0, sizeof(m_channelUsed));
    m_filterDirty         = false;
    m_lastCheckedSize     = 0;
    m_channelSummaryDirty = true;

    // Advance cursor past the now-cleared Logger ring buffer so we don't re-read old entries.
    m_readCursor = GetLogEntryCount();
    ClearLogHistory();
}

// ──────────────────────────────────────────────────────────────────────────────
// Cached summary strings
// ──────────────────────────────────────────────────────────────────────────────

void ConsoleWindow::RebuildLevelSummary()
{
    int active = 0;
    m_levelSummary.clear();

    for (int i = 0; i < static_cast<int>(LOG_LEVEL_MAX); ++i) {
        if (!showLevel[i]) continue;
        if (!m_levelSummary.empty()) m_levelSummary += ", ";
        m_levelSummary += k_LevelNames[i];
        ++active;
    }

    if (active == static_cast<int>(LOG_LEVEL_MAX)) m_levelSummary = "All Levels";
    else if (active == 0)                       m_levelSummary = "No Level Selected";
}

void ConsoleWindow::RebuildChannelSummary()
{
    int active = 0;
    m_channelSummary.clear();

    for (int i = 0; i < static_cast<int>(LogChannel::MAX_CHANNELS); ++i) {
        if (!showChannel[i]) continue;
        if (!m_channelSummary.empty()) m_channelSummary += ", ";
        m_channelSummary += LOG_CHANNEL_NAMES[i];
        ++active;
    }

    if (active == static_cast<int>(LogChannel::MAX_CHANNELS)) m_channelSummary = "All Channels";
    else if (active == 0)                        m_channelSummary = "No Channel Selected";
}

// ──────────────────────────────────────────────────────────────────────────────
// Draw
// ──────────────────────────────────────────────────────────────────────────────

void ConsoleWindow::Draw()
{
    if (!*p_open) return;

    // 1. Pull any new entries from the Logger (render thread only, no lock).
    PullNewEntries();

    // 2. Detect search text change — update lowercased needle and mark dirty.
    if (strcmp(searchBuffer, m_lastSearchStr) != 0) {
        strncpy(m_lastSearchStr, searchBuffer, sizeof(m_lastSearchStr) - 1);
        m_lastSearchStr[sizeof(m_lastSearchStr) - 1] = '\0';
        m_searchLower = searchBuffer;
        std::ranges::transform(m_searchLower,
                               m_searchLower.begin(),
                               [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
        m_filterDirty = true;
    }

    // 3. Rebuild or incrementally extend the filtered index list.
    if (m_filterDirty) {
        RebuildFilteredIndices();
    } else if (logBuffer.size() > m_lastCheckedSize) {
        UpdateFilteredIndicesIncremental(m_lastCheckedSize);
    }

    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(title, p_open, ImGuiWindowFlags_MenuBar))
    {
        DrawMenuBar();
        DrawLogPanel();
        DrawCommandLine();
    }
    ImGui::End();
}

// ──────────────────────────────────────────────────────────────────────────────
// Menu bar
// ──────────────────────────────────────────────────────────────────────────────

void ConsoleWindow::DrawMenuBar()
{
    if (!ImGui::BeginMenuBar()) return;

    if (ImGui::Button("Clear"))
        ClearDisplay();

    ImGui::SameLine(0, 50);
    ImGui::Checkbox("Auto-scroll", &autoScroll);
    ImGui::SameLine();
    if (ImGui::Checkbox("Freeze", &freezeConsole))
        SetLoggingPaused(freezeConsole);
    ImGui::SameLine();
    ImGui::Checkbox("Details", &m_showDetails);

    // ── Level filter ────────────────────────────────────────────────────────
    ImGui::SameLine(0, 50);
    ImGui::Text("Levels:");
    ImGui::SameLine();

    if (m_levelSummaryDirty) {
        RebuildLevelSummary();
        m_levelSummaryDirty = false;
    }

    ImGui::SetNextItemWidth(240);
    if (ImGui::BeginCombo("##LevelFilter", m_levelSummary.c_str()))
    {
        if (ImGui::Selectable("Select All", false)) {
            for (bool & i : showLevel)
                i = true;
            m_filterDirty = m_levelSummaryDirty = true;
        }
        if (ImGui::Selectable("Deselect All", false)) {
            for (bool & i : showLevel)
                i = false;
            m_filterDirty = m_levelSummaryDirty = true;
        }

        ImGui::Separator();

        for (int i = 0; i < static_cast<int>(LOG_LEVEL_MAX); ++i) {
            ImGui::PushStyleColor(ImGuiCol_Text, k_LevelColors[i]);
            if (ImGui::Selectable(k_LevelNames[i], &showLevel[i], ImGuiSelectableFlags_DontClosePopups)) {
                SetLogLevelEnabled(static_cast<LogLevel>(i), showLevel[i]);
                m_filterDirty = m_levelSummaryDirty = true;
            }
            ImGui::PopStyleColor();
        }

        ImGui::EndCombo();
    }

    // ── Channel filter ───────────────────────────────────────────────────────
    ImGui::SameLine(0, 50);
    ImGui::Text("Channels:");
    ImGui::SameLine();

    if (m_channelSummaryDirty) {
        RebuildChannelSummary();
        m_channelSummaryDirty = false;
    }

    ImGui::SetNextItemWidth(280);
    if (ImGui::BeginCombo("##ChannelFilter", m_channelSummary.c_str(), ImGuiComboFlags_HeightLargest))
    {
        if (ImGui::Selectable("Select All", false)) {
            for (int i = 0; i < static_cast<int>(LogChannel::MAX_CHANNELS); ++i)
                if (m_channelUsed[i]) showChannel[i] = true;
            m_filterDirty = m_channelSummaryDirty = true;
        }
        if (ImGui::Selectable("Deselect All", false)) {
            for (bool& b : showChannel) b = false;
            m_filterDirty = m_channelSummaryDirty = true;
        }

        ImGui::Separator();

        // Only show channels that have actually appeared in the log.
        for (int i = 0; i < static_cast<int>(LogChannel::MAX_CHANNELS); ++i) {
            if (!m_channelUsed[i]) continue;
            if (ImGui::Selectable(LOG_CHANNEL_NAMES[i], &showChannel[i], ImGuiSelectableFlags_DontClosePopups))
                m_filterDirty = m_channelSummaryDirty = true;
        }

        ImGui::EndCombo();
    }

    // ── Search ───────────────────────────────────────────────────────────────
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 350);
    ImGui::PushItemWidth(250);
    ImGui::InputTextWithHint("##Search", "Search logs...", searchBuffer, IM_ARRAYSIZE(searchBuffer));
    ImGui::PopItemWidth();

    ImGui::SameLine();
    ImGui::TextDisabled("%zu / %zu", m_filteredIndices.size(), logBuffer.size());

    ImGui::EndMenuBar();
}

// ──────────────────────────────────────────────────────────────────────────────
// Log panel — O(visible rows) per frame via ImGuiListClipper
// ──────────────────────────────────────────────────────────────────────────────

void ConsoleWindow::DrawLogPanel()
{
    const float footerHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footerHeight), false,
                      ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(m_filteredIndices.size()));

    while (clipper.Step())
    {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
        {
            const auto& [level, channel,
                timestamp, message, file,
                line, function, threadId]
            = logBuffer[m_filteredIndices[row]];

            // Format timestamp (seconds → MM:SS:mmm) — stack only, no allocation.
            const int totalMs = static_cast<int>(timestamp * 1000.0);
            char timeBuffer[16];
            snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d:%03d",
                     totalMs / 1000 / 60,
                     totalMs / 1000 % 60,
                     totalMs % 1000);

            // Strip "[LEVEL]: " prefix from the stored message — level is shown separately.
            const char* msgText = message.c_str();
            if (const char* sep = strstr(msgText, "]: ")) msgText = sep + 3;

            ImGui::PushStyleColor(ImGuiCol_Text, k_LevelColors[static_cast<int>(level)]);

            if (m_showDetails && (file || threadId != 0))
            {
                // Metadata prefix in a dimmer colour.
                // Format: [time] [LEVEL] [channel] file:line [func] (tid:N)
                ImGui::PopStyleColor();

                char metaBuffer[128];
                if (file)
                    snprintf(metaBuffer, sizeof(metaBuffer), "%s:%d [%s] (tid:%llu)",
                             GetFileName(file), line,
                             function ? function : "",
                             static_cast<unsigned long long>(threadId));
                else
                    snprintf(metaBuffer, sizeof(metaBuffer), "(tid:%llu)",
                             static_cast<unsigned long long>(threadId));

                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
                ImGui::Text("[%s] %s [%s] %s",
                            timeBuffer,
                            k_LevelNames[static_cast<int>(level)],
                            LOG_CHANNEL_NAMES[static_cast<int>(channel)],
                            metaBuffer);
                ImGui::PopStyleColor();

                // Message on the same row in level colour.
                ImGui::SameLine(0, 8);
                ImGui::PushStyleColor(ImGuiCol_Text, k_LevelColors[static_cast<int>(level)]);
                ImGui::TextUnformatted(msgText);
                ImGui::PopStyleColor();
            }
            else
            {
                ImGui::Text("[%s] %s [%s] %s",
                            timeBuffer,
                            k_LevelNames[static_cast<int>(level)],
                            LOG_CHANNEL_NAMES[static_cast<int>(channel)],
                            msgText);
                ImGui::PopStyleColor();
            }
        }
    }
    clipper.End();

    if (scrollToBottom && (autoScroll || ImGui::GetScrollY() >= ImGui::GetScrollMaxY())) {
        ImGui::SetScrollHereY(1.0f);
        scrollToBottom = false;
    }

    ImGui::PopStyleVar();
    ImGui::EndChild();
}

// ──────────────────────────────────────────────────────────────────────────────
// Command line
// ──────────────────────────────────────────────────────────────────────────────

void ConsoleWindow::DrawCommandLine()
{
    ImGui::Separator();

    bool reclaimFocus = false;

    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 150);
    if (ImGui::InputText("##Input", inputBuffer, IM_ARRAYSIZE(inputBuffer),
                         ImGuiInputTextFlags_EnterReturnsTrue))
    {
        // Trim leading/trailing whitespace in-place.
        char* start = inputBuffer;
        while (*start && (*start == ' ' || *start == '\t')) ++start;
        char* end = start + strlen(start) - 1;
        while (end > start && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) --end;
        *(end + 1) = '\0';

        if (start[0])
            ExecuteCommand(start);

        inputBuffer[0] = '\0';
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
        ClearDisplay();
    }
    else if (command == "help") {
        LogOutput(LOG_LEVEL_INFO, "Available commands:");
        LogOutput(LOG_LEVEL_INFO, "  clear, cls - Clear the console");
        LogOutput(LOG_LEVEL_INFO, "  help       - Show this help message");
        LogOutput(LOG_LEVEL_INFO, "  log_test   - Test all log levels");
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
        LogOutput(LOG_LEVEL_INFO,  "Type 'help' for available commands");
    }
}
