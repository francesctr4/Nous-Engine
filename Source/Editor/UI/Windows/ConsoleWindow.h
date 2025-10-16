#ifndef CONSOLEWINDOW_H
#define CONSOLEWINDOW_H

#include "Editor/UI/IEditorWindow.inl"
#include "Engine/Utils/Logger.h"
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

    // Use deque instead of vector for efficient front removal
    std::deque<std::pair<LogLevel, std::string>> logBuffer;
    bool autoScroll = true;
    bool scrollToBottom = false;

    // Filtering
    bool showLevel[6] = { true, true, true, true, true, false }; // FATAL, ERROR, WARN, INFO, DEBUG, TRACE

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
