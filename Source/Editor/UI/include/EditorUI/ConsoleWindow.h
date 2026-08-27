#pragma once

#include <EditorUI/IEditorWindow.h>
#include <Logger/Logger.h>
#include <utility>
#include <vector>
#include <string>

class ConsoleWindow : public IEditorWindow
{
public:
    explicit ConsoleWindow(const char* title, EditorContext* context, bool start_open = true);
    ~ConsoleWindow() override = default;

protected:

    void Init() override;
    void Update() override;
    ImGuiWindowFlags GetWindowFlags() const override;
    void DrawContent() override;

private:

    void DrawChannelFilterSection();
    void DrawSearchInputField();
    void DrawLevelFilterSection();
    void DrawControlsSection();
    void DrawMenuBar();
    void DrawLogRow(int row);
    void DrawLogPanel();
    void DrawCommandLine();
    void ExecuteCommand(const std::string& command);

    // Called once per frame before drawing — pulls new entries from Logger.
    void PullNewEntries();

    // Filtered index list management.
    void RebuildFilteredIndices();
    void UpdateFilteredIndicesIncremental(size_t fromIndex);
    bool PassesFilters(const LogEntry& entry) const;

    // Clears both the display buffer and the Logger ring buffer.
    void ClearDisplay();

    // Summary string helpers (rebuilt only when the matching dirty flag is set).
    void RebuildLevelSummary();
    void RebuildChannelSummary();

    // ---------------------------------------------------------------------------------------
    // ---------------------------------------------------------------------------------------
    // ---------------------------------------------------------------------------------------

    // ── Log data ────────────────────────────────────────────────────────────
    // Only ever touched on the render/main thread (pull model — no mutex needed).
    std::vector<LogEntry> logBuffer;
    uint64_t m_readCursor = 0; // cursor into Logger ring buffer

    // ── Filtered view ────────────────────────────────────────────────────────
    std::vector<int> m_filteredIndices;
    bool m_filterDirty = true; // full rebuild needed
    size_t m_lastCheckedSize = 0; // logBuffer size at last incremental update

    // ── Channel presence (incremental) ──────────────────────────────────────
    std::array<bool, static_cast<int>(std::to_underlying(LogChannel::MAX_CHANNELS))> m_channelUsed;

    // ── Cached UI strings ───────────────────────────────────────────────────
    std::string m_levelSummary;
    std::string m_channelSummary;
    bool m_levelSummaryDirty = true;
    bool m_channelSummaryDirty = true;

    // ── Search state ─────────────────────────────────────────────────────────
    std::string searchBuffer;
    std::string m_lastSearchStr; // detect changes between frames
    std::string m_searchLower; // pre-lowercased needle (no per-entry alloc)

    // ── Scroll ───────────────────────────────────────────────────────────────
    bool autoScroll = true;
    bool scrollToBottom = false;

    // ── Level / channel filters ──────────────────────────────────────────────
    std::array<bool, static_cast<int>(std::to_underlying(LOG_LEVEL_MAX))> showLevel = {true, true, true, true, true, false};
    std::array<bool, static_cast<int>(std::to_underlying(LogChannel::MAX_CHANNELS))> showChannel;
    // all initialised to true in ctor

    // ── Command line ─────────────────────────────────────────────────────────
    std::vector<std::string> commandHistory;
    int historyPos = -1;
    std::string inputBuffer;

    bool freezeConsole = false;
    bool m_showDetails = true; // show file:line + thread ID per entry
};
