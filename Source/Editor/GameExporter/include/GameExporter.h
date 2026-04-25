#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct GameExportConfig
{
    std::string outputPath;
    std::string startupScene;   // Library/Scenes/{name}.nous — empty = keep original
    bool        launchAfter = false;
};

class GameExporter
{
public:
    GameExporter()  = default;
    ~GameExporter();

    void StartExport(GameExportConfig config);
    void Cancel();

    bool IsRunning()      const { return m_running.load(); }
    int  GetCurrentStep() const { return m_currentStep.load(); }

    void DrainLog(std::vector<std::string>& outLines);
    bool PollDone(bool& outSuccess);

private:
    void RunPipeline(const GameExportConfig& config);
    void PostLog(const std::string& line);
    void PostDone(bool success);

    std::atomic<bool>        m_running         { false };
    std::atomic<bool>        m_cancelRequested { false };
    std::atomic<int>         m_currentStep     { 0 };

    std::mutex               m_logMutex;
    std::vector<std::string> m_pendingLogLines;

    std::mutex               m_doneMutex;
    bool                     m_done    = false;
    bool                     m_success = false;

    std::thread              m_buildThread;
};
