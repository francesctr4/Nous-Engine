#include "Editor/GameExporter/include/GameExporter.h"

#include "Engine/Utils/Serialization/JsonFile/JsonFile.h"
#include "Engine/Utils/Serialization/JsonFile/JsonObject.h"

#include <filesystem>
#include <functional>
#include <unordered_set>

#include <SDL3/SDL.h>

#ifdef _WIN32
#include <Windows.h>
#include <shellapi.h>
#else
#include <cstdio>
#include <sys/wait.h>
#endif

// ─── Internal helpers ────────────────────────────────────────────────────────

namespace
{
    std::filesystem::path ResolveEngineDir()
    {
        const char* base = SDL_GetBasePath();
        std::filesystem::path p = base ? base : ".";
        std::string s = p.string();
        if (!s.empty() && (s.back() == '/' || s.back() == '\\'))
            s.pop_back();
        return s;
    }

    // In an engine delivery, Library/ sits next to the executable (engineDir/Library/Shaders/ exists).
    // In a dev build (CLion), the exe is in Build/.../bin/ but Library/ lives at the project root (cwd).
    std::filesystem::path ResolveLibraryDir(const std::filesystem::path& engineDir)
    {
        const auto deliveryLib = engineDir / "Library";
        // Library/GameBin/GameApp only exists in a proper engine delivery (InstallEngine).
        // In dev builds CMake copies Shaders to bin/Library/ but not the full content,
        // so checking for Shaders/ gives a false positive — use GameBin as the indicator instead.
        const std::string gameExeName = "GameApp" + std::string(GameExporterPlatform::GetExeExtension());
        if (std::filesystem::exists(deliveryLib / "GameBin" / gameExeName))
            return deliveryLib;
        return std::filesystem::current_path() / "Library";
    }

    // game_config.json source resolution:
    // - Delivery: Library/Settings/game_config.json next to the executable
    // - Dev build: Source/Game/game_config.json at the project root (cwd)
    std::filesystem::path ResolveGameConfigSource(const std::filesystem::path& engineDir)
    {
        const auto deliveryCfg = engineDir / "Library" / "Settings" / "game_config.json";
        if (std::filesystem::exists(deliveryCfg))
            return deliveryCfg;
        const auto libCfg = std::filesystem::current_path() / "Library" / "Settings" / "game_config.json";
        if (std::filesystem::exists(libCfg))
            return libCfg;
        return std::filesystem::current_path() / "Source" / "Game" / "game_config.json";
    }

#ifdef _WIN32
    bool LaunchAndCapture(const std::string& cmd,
                          const std::function<void(const std::string&)>& onLine,
                          std::atomic<bool>& cancelFlag)
    {
        SECURITY_ATTRIBUTES sa = {};
        sa.nLength        = sizeof(sa);
        sa.bInheritHandle = TRUE;

        HANDLE hRead, hWrite;
        if (!CreatePipe(&hRead, &hWrite, &sa, 0))
            return false;
        SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOA si = {};
        si.cb         = sizeof(si);
        si.hStdOutput = hWrite;
        si.hStdError  = hWrite;
        si.dwFlags    = STARTF_USESTDHANDLES;

        PROCESS_INFORMATION pi = {};
        std::string cmdBuf     = cmd;
        const BOOL ok = CreateProcessA(nullptr, cmdBuf.data(), nullptr, nullptr,
                                       TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
        CloseHandle(hWrite);
        if (!ok) { CloseHandle(hRead); return false; }

        char       buf[1024];
        std::string lineAccum;
        DWORD      bytesRead;
        while (ReadFile(hRead, buf, sizeof(buf) - 1, &bytesRead, nullptr) && bytesRead > 0)
        {
            if (cancelFlag.load()) break;
            buf[bytesRead] = '\0';
            lineAccum += buf;
            size_t pos;
            while ((pos = lineAccum.find('\n')) != std::string::npos)
            {
                std::string line = lineAccum.substr(0, pos);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (!line.empty()) onLine(line);
                lineAccum.erase(0, pos + 1);
            }
        }
        if (!lineAccum.empty()) onLine(lineAccum);

        CloseHandle(hRead);
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode = 1;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return exitCode == 0;
    }
#else
    bool LaunchAndCapture(const std::string& cmd,
                          const std::function<void(const std::string&)>& onLine,
                          std::atomic<bool>& cancelFlag)
    {
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) return false;
        char buf[1024];
        std::string lineAccum;
        while (!cancelFlag.load() && fgets(buf, sizeof(buf), pipe))
        {
            lineAccum += buf;
            size_t pos;
            while ((pos = lineAccum.find('\n')) != std::string::npos)
            {
                std::string line = lineAccum.substr(0, pos);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (!line.empty()) onLine(line);
                lineAccum.erase(0, pos + 1);
            }
        }
        if (!lineAccum.empty()) onLine(lineAccum);
        // NOTE: pclose() calls waitpid() and blocks until the child process exits.
        // If cancelFlag fired, the loop exits early but the child is still running,
        // so Cancel() will not return immediately on Linux/macOS. A full fix requires
        // fork+exec with SIGTERM on the child PID; popen does not expose the PID.
        const int status = pclose(pipe);
        return status != -1 && WIFEXITED(status) && WEXITSTATUS(status) == 0;
    }
#endif

    bool CopyLibraryContents(const std::filesystem::path& src,
                              const std::filesystem::path& dst,
                              std::error_code& ec)
    {
        std::filesystem::create_directories(dst, ec);
        if (ec) return false;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(src, ec))
        {
            if (ec) return false;
            const auto rel    = std::filesystem::relative(entry.path(), src, ec);
            if (ec) return false;
            const std::string relStr = rel.generic_string();

            if (relStr.starts_with("Scripts") || relStr.starts_with("GameBin"))
                continue;
            if (entry.path().filename().string() == "_simulation_snapshot.nous")
                continue;

            const auto target = dst / rel;
            if (entry.is_directory())
                std::filesystem::create_directories(target, ec);
            else
                std::filesystem::copy_file(entry.path(), target,
                                           std::filesystem::copy_options::overwrite_existing, ec);
            if (ec) return false;
        }
        return true;
    }
} // namespace

// ─── GameExporterPlatform ─────────────────────────────────────────────────────

bool GameExporterPlatform::CopySharedLibs(const std::filesystem::path& srcDir,
                                          const std::filesystem::path& dstDir,
                                          std::error_code& ec,
                                          const std::unordered_set<std::string>& excludeStems)
{
    if (!std::filesystem::exists(srcDir, ec) || ec)
    {
        if (!ec) ec = std::make_error_code(std::errc::no_such_file_or_directory);
        return false;
    }
    std::filesystem::create_directories(dstDir, ec);
    if (ec) return false;
    for (const auto& entry : std::filesystem::directory_iterator(srcDir, ec))
    {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != GetSharedLibExt()) continue;
        if (!excludeStems.empty())
        {
            const std::string stem = entry.path().stem().string();
            if (excludeStems.count(stem)) continue;
        }
        std::filesystem::copy_file(entry.path(), dstDir / entry.path().filename(),
                                   std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) return false;
    }
    return true;
}

bool GameExporterPlatform::PatchGameConfig(const std::filesystem::path& configPath,
                                           const std::string& startScene)
{
    JsonObject cfg = JsonFile::LoadFromFile(configPath.string());
    if (cfg.IsEmpty()) return false;
    cfg.Set("startScene", startScene);
    return JsonFile::SaveToFile(cfg, configPath.string());
}

// ─── GameExporter public API ──────────────────────────────────────────────────

GameExporter::~GameExporter()
{
    Cancel();
    if (m_buildThread.joinable())
        m_buildThread.join();
}

void GameExporter::StartExport(GameExportConfig config)
{
    if (m_running.load()) return;

    m_running.store(true);
    m_cancelRequested.store(false);
    m_currentStep.store(0);

    { std::lock_guard lock(m_logMutex);  m_pendingLogLines.clear(); }
    { std::lock_guard lock(m_doneMutex); m_done = false; m_success = false; }

    if (m_buildThread.joinable())
        m_buildThread.join();

    m_buildThread = std::thread([this, cfg = std::move(config)]() {
        RunPipeline(cfg);
    });
}

void GameExporter::Cancel()
{
    m_cancelRequested.store(true);
}

void GameExporter::DrainLog(std::vector<std::string>& outLines)
{
    std::lock_guard lock(m_logMutex);
    outLines.insert(outLines.end(), m_pendingLogLines.begin(), m_pendingLogLines.end());
    m_pendingLogLines.clear();
}

bool GameExporter::PollDone(bool& outSuccess)
{
    std::lock_guard lock(m_doneMutex);
    if (!m_done) return false;
    outSuccess = m_success;
    m_done     = false;
    return true;
}

void GameExporter::PostLog(const std::string& line)
{
    std::lock_guard lock(m_logMutex);
    m_pendingLogLines.push_back(line);
}

void GameExporter::PostDone(bool success)
{
    m_running.store(false);
    std::lock_guard lock(m_doneMutex);
    m_done    = true;
    m_success = success;
}

void GameExporter::RunPipeline(const GameExportConfig& config)
{
    const std::filesystem::path engineDir = ResolveEngineDir();
    const std::filesystem::path outputDir = config.outputPath;
    std::error_code ec;

    auto abort = [&](const std::string& msg) {
        PostLog("[ERROR] " + msg);
        PostDone(false);
    };

    auto checkCancel = [&]() -> bool {
        if (!m_cancelRequested.load()) return false;
        std::filesystem::remove_all(outputDir, ec);
        PostLog("[CANCELLED] Build cancelled.");
        PostDone(false);
        return true;
    };

    // ── Step 1: Clear and create output directory ─────────────────────────
    m_currentStep.store(1);
    PostLog("[INFO] Clearing output directory: " + outputDir.string());
    std::filesystem::remove_all(outputDir, ec);
    if (ec) { abort("Failed to clear output dir: " + ec.message()); return; }
    std::filesystem::create_directories(outputDir, ec);
    if (ec) { abort("Failed to create output dir: " + ec.message()); return; }
    if (checkCancel()) return;

    // ── Step 2: Recompile scripts ─────────────────────────────────────────
    m_currentStep.store(2);
    PostLog("[INFO] Recompiling scripts...");
    {
#ifdef NDEBUG
        const std::string buildMode = "Release";
#else
        const std::string buildMode = "Debug";
#endif

#ifdef _WIN32
        const std::string scriptPath =
            (engineDir / "Library" / "Scripts" / "RebuildScripts.bat").string();
        const std::string cmd = "\"" + scriptPath + "\" " + buildMode;
#else
        const std::string scriptPath =
            (engineDir / "Library" / "Scripts" / "RebuildScripts.sh").string();
        const std::string cmd = "bash \"" + scriptPath + "\" " + buildMode + " 2>&1";
#endif
        const bool ok = LaunchAndCapture(cmd,
            [&](const std::string& line) { PostLog(line); },
            m_cancelRequested);
        if (!ok)
        {
            if (checkCancel()) return;
            abort("Script recompilation failed — see log above.");
            return;
        }
    }
    if (checkCancel()) return;

    // ── Step 3: Copy engine shared libs ───────────────────────────────────
    m_currentStep.store(3);
    PostLog("[INFO] Copying engine binaries...");
    // Exclude editor-only libs — the game runtime does not depend on them.
    static const std::unordered_set<std::string> c_editorOnlyLibs = { "Nous-Editor", "libNous-Editor" };
    if (!GameExporterPlatform::CopySharedLibs(engineDir, outputDir, ec, c_editorOnlyLibs))
    { abort("Failed to copy shared libs: " + ec.message()); return; }
    if (checkCancel()) return;

    // ── Step 4: Copy GameApp executable ───────────────────────────────────
    m_currentStep.store(4);
    {
        const std::string gameExeName = "GameApp" + std::string(GameExporterPlatform::GetExeExtension());
        PostLog("[INFO] Copying " + gameExeName + "...");
        // Engine delivery: Library/GameBin/<gameExeName>
        // Dev build: <gameExeName> sits next to EditorApp in bin/
        const auto deliveryPath = engineDir / "Library" / "GameBin" / gameExeName;
        const auto devPath      = engineDir / gameExeName;
        const auto src = std::filesystem::exists(deliveryPath) ? deliveryPath : devPath;
        const auto dst = outputDir / gameExeName;
        std::filesystem::copy_file(src, dst,
            std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) { abort("Failed to copy " + gameExeName + ": " + ec.message()); return; }
    }
    if (checkCancel()) return;

    // ── Step 5: Copy freshly compiled Scripts shared lib ──────────────────
    m_currentStep.store(5);
    {
        const auto scriptLibName = GameExporterPlatform::GetScriptLibName();
        PostLog("[INFO] Copying " + std::string(scriptLibName) + "...");
        const auto src    = engineDir / "Library" / "Scripts" / scriptLibName;
        const auto dstDir = outputDir / "Library" / "Scripts";
        std::filesystem::create_directories(dstDir, ec);
        if (ec) { abort("Failed to create Library/Scripts: " + ec.message()); return; }
        std::filesystem::copy_file(src, dstDir / scriptLibName,
            std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) { abort("Failed to copy " + std::string(scriptLibName) + ": " + ec.message()); return; }
    }
    if (checkCancel()) return;

    // ── Step 6: Mirror Library/ (excluding Scripts/ and GameBin/) ────────
    m_currentStep.store(6);
    PostLog("[INFO] Copying Library...");
    {
        const auto libSrc = ResolveLibraryDir(engineDir);
        const auto libDst = outputDir / "Library";
        PostLog("[INFO] Library source: " + libSrc.string());
        if (!CopyLibraryContents(libSrc, libDst, ec))
        { abort("Failed to copy Library: " + ec.message()); return; }
    }
    if (checkCancel()) return;

    // ── Step 7: Copy and patch game_config.json ───────────────────────────
    m_currentStep.store(7);
    {
        const auto cfgSrc    = ResolveGameConfigSource(engineDir);
        const auto cfgDstDir = outputDir / "Library" / "Settings";
        const auto cfgDst    = cfgDstDir / "game_config.json";

        std::filesystem::create_directories(cfgDstDir, ec);
        if (!ec)
            std::filesystem::copy_file(cfgSrc, cfgDst,
                std::filesystem::copy_options::overwrite_existing, ec);

        if (ec)
            PostLog("[WARN] Could not copy game_config.json: " + ec.message());
        else if (!config.startupScene.empty())
        {
            PostLog("[INFO] Setting startup scene: " + config.startupScene);
            if (!GameExporterPlatform::PatchGameConfig(cfgDst, config.startupScene))
                PostLog("[WARN] Could not patch game_config.json — startup scene unchanged.");
        }
    }

    PostLog("[OK] Build succeeded -> " + outputDir.string());
    PostDone(true);

    if (config.launchAfter)
    {
        const std::string exePath =
            (outputDir / ("GameApp" + std::string(GameExporterPlatform::GetExeExtension()))).string();
#ifdef _WIN32
        ShellExecuteA(nullptr, "open", exePath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#elif defined(__APPLE__)
        if (system(("open \"" + exePath + "\"").c_str()) != 0)
            PostLog("[WARN] Could not launch game — 'open' returned non-zero.");
#else
        if (system(("xdg-open \"" + exePath + "\" &").c_str()) != 0)
            PostLog("[WARN] Could not launch game — 'xdg-open' returned non-zero.");
#endif
    }
}
