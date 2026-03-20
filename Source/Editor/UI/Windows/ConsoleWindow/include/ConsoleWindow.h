#ifndef CONSOLEWINDOW_H
#define CONSOLEWINDOW_H

#include "Editor/UI/IEditorWindow.inl"
#include "Engine/Core/Logger/Logger.h"
#include <vector>
#include <string>

class ConsoleWindow : public IEditorWindow
{
public:
    explicit ConsoleWindow(const char* title, EditorContext* context, bool start_open = true);
    ~ConsoleWindow() override = default;

    void Init() override;
    void Draw() override;

private:
    void DrawMenuBar();
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

    // ── Log data ────────────────────────────────────────────────────────────
    // Only ever touched on the render/main thread (pull model — no mutex needed).
    std::vector<LogEntry> logBuffer;
    uint64_t              m_readCursor      = 0;    // cursor into Logger ring buffer

    // ── Filtered view ────────────────────────────────────────────────────────
    std::vector<int> m_filteredIndices;
    bool             m_filterDirty     = true;  // full rebuild needed
    size_t           m_lastCheckedSize = 0;     // logBuffer size at last incremental update

    // ── Channel presence (incremental) ──────────────────────────────────────
    bool m_channelUsed[(int)LogChannel::MAX_CHANNELS] = {};

    // ── Cached UI strings ───────────────────────────────────────────────────
    std::string m_levelSummary;
    std::string m_channelSummary;
    bool        m_levelSummaryDirty   = true;
    bool        m_channelSummaryDirty = true;

    // ── Search state ─────────────────────────────────────────────────────────
    char        searchBuffer[256]    = "";
    char        m_lastSearchStr[256] = ""; // detect changes between frames
    std::string m_searchLower;             // pre-lowercased needle (no per-entry alloc)

    // ── Scroll ───────────────────────────────────────────────────────────────
    bool autoScroll    = true;
    bool scrollToBottom = false;

    // ── Level / channel filters ──────────────────────────────────────────────
    bool showLevel  [(int)LogLevel::LOG_LEVEL_MAX]   = { true, true, true, true, true, false };
    bool showChannel[(int)LogChannel::MAX_CHANNELS]  = {};  // all initialised to true in ctor

    // ── Command line ─────────────────────────────────────────────────────────
    std::vector<std::string> commandHistory;
    int  historyPos   = -1;
    char inputBuffer[256] = "";

    bool freezeConsole = false;
    bool m_showDetails = true; // show file:line + thread ID per entry

    static constexpr const char* k_LevelNames[] = {
        "FATAL", "ERROR", "WARN", "INFO", "DEBUG", "TRACE"
    };

    static constexpr size_t k_MaxDisplayEntries = 10000;
};

#endif // CONSOLEWINDOW_H
