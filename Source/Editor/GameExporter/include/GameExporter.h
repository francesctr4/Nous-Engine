#pragma once

#include <atomic>
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <unordered_set>
#include <vector>

namespace GameExporterPlatform
{
#ifdef _WIN32
    constexpr std::string_view GetExeExtension()  { return ".exe";         }
    constexpr std::string_view GetSharedLibExt()  { return ".dll";         }
    constexpr std::string_view GetScriptLibName() { return "Scripts.dll";  }
#elif defined(__APPLE__)
    constexpr std::string_view GetExeExtension()  { return "";              }
    constexpr std::string_view GetSharedLibExt()  { return ".dylib";        }
    constexpr std::string_view GetScriptLibName() { return "Scripts.dylib"; }
#else
    constexpr std::string_view GetExeExtension()  { return "";             }
    constexpr std::string_view GetSharedLibExt()  { return ".so";          }
    constexpr std::string_view GetScriptLibName() { return "Scripts.so";   }
#endif

    bool CopySharedLibs(const std::filesystem::path& srcDir,
                        const std::filesystem::path& dstDir,
                        std::error_code& ec,
                        const std::unordered_set<std::string>& excludeStems = {});

    bool PatchGameConfig(const std::filesystem::path& configPath,
                         const std::string& startScene);
} // namespace GameExporterPlatform

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
