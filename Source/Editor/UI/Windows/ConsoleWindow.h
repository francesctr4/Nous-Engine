#ifndef CONSOLEWINDOW_H
#define CONSOLEWINDOW_H

#include "Editor/UI/IEditorWindow.inl"
#include "Engine/Utils/Logging System/Logger.h"
#include <vector>
#include <string>
#include <functional>
#include <deque>

class ConsoleWindow : public IEditorWindow
{
public:
    explicit ConsoleWindow(const char* title, bool start_open = true);
    virtual ~ConsoleWindow();

    void Init() override;
    void Draw() override;

private:
    void DrawMenuBar();
    void DrawLogPanel();
    void DrawCommandLine();
    void ExecuteCommand(const std::string& command);

    // Updated buffer to store channel info
    std::deque<std::tuple<LogLevel, LogChannel, std::string>> logBuffer;

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
