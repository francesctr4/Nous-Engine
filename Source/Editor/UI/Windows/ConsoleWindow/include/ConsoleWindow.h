#ifndef CONSOLEWINDOW_H
#define CONSOLEWINDOW_H

#include "Editor/UI/IEditorWindow.inl"
#include "Engine/Core/Logger/Logger.h"
#include <vector>
#include <string>
#include <functional>
#include <deque>
#include <mutex>

class ConsoleWindow : public IEditorWindow
{
public:
    explicit ConsoleWindow(const char* title, EditorContext* context, bool start_open = true);
    ~ConsoleWindow() override;

    void Init() override;
    void Draw() override;

private:
    void DrawMenuBar();
    void DrawLogPanel();
    void DrawCommandLine();
    void ExecuteCommand(const std::string& command);

    std::mutex logMutex;
    std::deque<std::tuple<LogLevel, LogChannel, double, std::string>> logBuffer;

    bool autoScroll = true;
    bool scrollToBottom = false;

    // Filtering
    bool showLevel[(int)LogLevel::LOG_LEVEL_MAX] = { true, true, true, true, true, false };
    bool showChannel[(int)LogChannel::MAX_CHANNELS] = { true }; // new!

    // Command history
    std::vector<std::string> commandHistory;
    int historyPos = -1;
    char inputBuffer[256] = "";

    const char* levelNames[6] = {
            "FATAL", "ERROR", "WARN", "INFO", "DEBUG", "TRACE"
    };

    bool freezeConsole = false;

    char searchBuffer[256] = "";
};

#endif // CONSOLEWINDOW_H
