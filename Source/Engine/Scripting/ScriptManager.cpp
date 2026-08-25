#include <Engine/Scripting/ScriptManager.h>
#include <Engine/Scripting/Internal/ScriptRegistry.inl>
#include "Engine/Core/Logger/Logger.h"
#include <Engine/Core/MemoryManager/MemoryManager.h>
#include "Engine/NOUS_Multithreading/NOUS_Multithreading.h"
#include "Engine/NOUS_Multithreading/NOUS_Thread/include/NOUS_Thread.h"
#include <Engine/Scripting/EngineAPI/EngineAPI.h>
#include <Engine/Scripting/EngineAPI/Bindings/ScriptBindings.h>
#include "Engine/Systems/ECS/Component/Types/CScript/include/CScript.h"

#include <fstream>
#include <sstream>
#include <filesystem>

#ifdef _WIN32
#  include <Windows.h>
#elif defined(__APPLE__)
#  include <dlfcn.h>
#  include <mach-o/dyld.h>   // _NSGetExecutablePath
#  include <climits>          // PATH_MAX
#else
#  include <dlfcn.h>
#endif

#ifdef _WIN32

static std::wstring ToWide(const std::string& s)
{
    if (s.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), size);
    return w;
}

// Runs: cmd.exe /c "<commandLine>"
// Captures stdout+stderr live (merged) and returns process exit code.
// onLine: called for every line (without \r\n)
static bool RunProcessCaptureLive(const std::wstring& commandLine, DWORD& outExitCode,
                                  const std::function<void(const std::string&)>& onLine)
{
    outExitCode = (DWORD)-1;

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE readPipe = nullptr;
    HANDLE writePipe = nullptr;

    if (!CreatePipe(&readPipe, &writePipe, &sa, 0))
        return false;

    // Ensure the read end is not inherited.
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = writePipe;
    si.hStdError  = writePipe;
    si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi{};

    // Use cmd.exe so .bat works
    std::wstring full = L"cmd.exe /c \"" + commandLine + L"\"";

    // CreateProcessW requires a mutable buffer
    std::vector<wchar_t> cmdBuf(full.begin(), full.end());
    cmdBuf.push_back(L'\0');

    const BOOL ok = CreateProcessW(
            nullptr,
            cmdBuf.data(),
            nullptr,
            nullptr,
            TRUE,                // inherit handles (so child gets writePipe)
            CREATE_NO_WINDOW,    // no extra console window
            nullptr,
            nullptr,
            &si,
            &pi
    );

    // Parent no longer needs write end
    CloseHandle(writePipe);

    if (!ok)
    {
        CloseHandle(readPipe);
        return false;
    }

    std::string pending;
    char buffer[4096];

    // Read until process exits and pipe drains
    while (true)
    {
        DWORD bytesAvail = 0;
        if (PeekNamedPipe(readPipe, nullptr, 0, nullptr, &bytesAvail, nullptr) && bytesAvail > 0)
        {
            DWORD bytesRead = 0;
            if (ReadFile(readPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0)
            {
                buffer[bytesRead] = 0;
                pending.append(buffer, bytesRead);

                // Emit complete lines
                for (;;)
                {
                    const size_t pos = pending.find('\n');
                    if (pos == std::string::npos) break;

                    std::string line = pending.substr(0, pos);
                    if (!line.empty() && line.back() == '\r') line.pop_back();
                    pending.erase(0, pos + 1);

                    if (onLine) onLine(line);
                }
            }
        }

        // Check if process ended
        DWORD wait = WaitForSingleObject(pi.hProcess, 15);
        if (wait == WAIT_OBJECT_0)
            break;
    }

    // Drain remaining output
    for (;;)
    {
        DWORD bytesRead = 0;
        if (!ReadFile(readPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) || bytesRead == 0)
            break;

        buffer[bytesRead] = 0;
        pending.append(buffer, bytesRead);

        for (;;)
        {
            const size_t pos = pending.find('\n');
            if (pos == std::string::npos) break;

            std::string line = pending.substr(0, pos);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            pending.erase(0, pos + 1);

            if (onLine) onLine(line);
        }
    }

    if (!pending.empty() && onLine)
        onLine(pending);

    GetExitCodeProcess(pi.hProcess, &outExitCode);

    CloseHandle(readPipe);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    return true;
}

#endif // _WIN32

// Returns the directory containing the running executable (no trailing separator).
static std::filesystem::path GetExeDir()
{
#ifdef _WIN32
    char buf[MAX_PATH];
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    return std::filesystem::path(buf).parent_path();
#elif defined(__APPLE__)
    char buf[PATH_MAX];
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) == 0)
        return std::filesystem::canonical(buf).parent_path();
    return std::filesystem::current_path();
#else
    return std::filesystem::canonical("/proc/self/exe").parent_path();
#endif
}

ScriptManager::ScriptManager(IScriptInput* input, IScriptSceneHost* sceneHost)
    : m_libraryHandle(nullptr), m_shadowDllPath(), m_scriptRegistry(nullptr),
      m_input(input), m_sceneHost(sceneHost),
      m_scriptComponents(MemoryTag::SCRIPTING_SYSTEM)
{
    ScriptBindings::InitializeBindings(api);
}

ScriptManager::~ScriptManager()
{
    UnloadScriptLibrary();
    ScriptBindings::DeleteBindings(api);
}

bool ScriptManager::LoadScriptLibrary(const std::string& dllPath) {
    UnloadScriptLibrary();

#ifdef _WIN32
    // Shadow-copy the DLL so the original is never locked by this process.
    // The linker can then overwrite Scripts.dll freely on the next hot-reload.
    const std::filesystem::path src(dllPath);

    if (!std::filesystem::exists(src))
    {
        NOUS_WARN("Script library not found at '%s' — scripting disabled", dllPath.c_str());
        return false;
    }

    const std::filesystem::path shadow = src.parent_path() / (src.stem().string() + "_shadow" + src.extension().string());

    std::error_code ec;
    std::filesystem::create_directories(src.parent_path(), ec);
    std::filesystem::copy_file(src, shadow, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec)
    {
        NOUS_ERROR("Failed to create shadow copy of script DLL: %s", ec.message().c_str());
        return false;
    }
    m_shadowDllPath = shadow.string();
    m_libraryHandle = LoadDLL(m_shadowDllPath);
#else
    m_libraryHandle = LoadDLL(dllPath);
#endif

    if (!m_libraryHandle) {
        NOUS_ERROR("Failed to load script library: %s", dllPath.c_str());
        return false;
    }

    // Get the registry function
    auto getRegistryFunc = reinterpret_cast<ScriptRegistry*(*)()>(
            GetSymbol(m_libraryHandle, "GetScriptRegistry"));

    if (!getRegistryFunc) {
        NOUS_ERROR("Failed to find GetScriptRegistry in script library");
        UnloadScriptLibrary();
        return false;
    }

    m_scriptRegistry = getRegistryFunc();
    if (!m_scriptRegistry) {
        NOUS_ERROR("Failed to get script registry");
        UnloadScriptLibrary();
        return false;
    }

    ScriptBindings::SetupAllBindings(*api, m_input, m_sceneHost);

    // Set the API pointer inside the scripts DLL
    using SetEngineAPIFunc = void(*)(EngineAPI*);
    auto setEngineAPIFunc = reinterpret_cast<SetEngineAPIFunc>(
            GetSymbol(m_libraryHandle, "SetEngineAPI")
    );

    if (setEngineAPIFunc) {
        setEngineAPIFunc(api);
    } else {
        NOUS_ERROR("Failed to find SetEngineAPI in script library");
    }

    NOUS_INFO("Script library loaded successfully: %s", dllPath.c_str());
    return true;
}

void ScriptManager::UnloadScriptLibrary() {
    if (m_libraryHandle)
    {
        m_scriptRegistry = nullptr;

        UnloadLibrary(m_libraryHandle);
        m_libraryHandle = nullptr;

        // Small delay to ensure the OS fully releases the file handle.
        nous::engine::multithreading::NOUS_Thread::SleepMS(100);

#ifdef _WIN32
        // Delete the shadow copy so it can be recreated fresh on the next load.
        if (!m_shadowDllPath.empty())
        {
            std::error_code ec;
            std::filesystem::remove(m_shadowDllPath, ec);
            if (ec)
                NOUS_WARN("Could not delete shadow DLL '%s': %s", m_shadowDllPath.c_str(), ec.message().c_str());
            m_shadowDllPath.clear();
        }
#endif
    }
}

bool ScriptManager::ReloadScriptLibrary(const std::string& dllPath)
{
    NOUS_INFO("Reloading script library...");
    UnloadScriptLibrary();

#ifdef _WIN32
    const std::wstring batPath = (GetExeDir() / "EngineCore" / "Scripts" / "RebuildScripts.bat").wstring();
    std::wstring cmd;
#ifdef _DEBUG
    cmd = batPath + L" Debug";
#else
    cmd = batPath + L" Release";
#endif

    // Track which specific failure the .bat reported, so we can emit a
    // human-readable explanation (and install hints) on non-zero exit.
    enum class BuildFailureCause
    {
        None,
        VswhereMissing,             // VS Installer not present
        VsCppToolsMissing,          // VS found, but C++ workload not installed
        Vcvars64Missing,            // VS install corrupted / partial
        VcvarsInitFailed,           // vcvars64.bat returned non-zero
        SdkHeadersMissing,          // engine packaging issue (not user-fixable)
        SdkEngineApiMissing,        // engine packaging issue (not user-fixable)
        ScriptsFolderMissing,       // no Assets/Scripts directory
        CompilationFailed           // user script compile error
    };
    BuildFailureCause failureCause = BuildFailureCause::None;

    DWORD exitCode = 0;
    bool ran = RunProcessCaptureLive(cmd, exitCode, [&failureCause](const std::string& line)
    {
        // Classify [ERROR] markers emitted by RebuildScripts.bat.
        if (line.find("[ERROR]") != std::string::npos)
        {
            if (line.find("vswhere.exe not found") != std::string::npos)
                failureCause = BuildFailureCause::VswhereMissing;
            else if (line.find("Visual Studio C++ Build Tools not found") != std::string::npos)
                failureCause = BuildFailureCause::VsCppToolsMissing;
            else if (line.find("vcvars64.bat not found") != std::string::npos)
                failureCause = BuildFailureCause::Vcvars64Missing;
            else if (line.find("Failed to initialize MSVC environment") != std::string::npos)
                failureCause = BuildFailureCause::VcvarsInitFailed;
            else if (line.find("Script SDK headers missing") != std::string::npos)
                failureCause = BuildFailureCause::SdkHeadersMissing;
            else if (line.find("Script SDK EngineAPI.cpp missing") != std::string::npos)
                failureCause = BuildFailureCause::SdkEngineApiMissing;
            else if (line.find("Scripts source folder missing") != std::string::npos)
                failureCause = BuildFailureCause::ScriptsFolderMissing;
            else if (line.find("Script compilation failed") != std::string::npos)
                failureCause = BuildFailureCause::CompilationFailed;
        }

        if (line.find("error") != std::string::npos || line.find("fatal") != std::string::npos)
            NOUS_ERROR("[ScriptManager::ReloadScriptLibrary] %s", line.c_str());
        else
            NOUS_INFO("[ScriptManager::ReloadScriptLibrary] %s", line.c_str());
    });

    if (!ran)
    {
        NOUS_ERROR("Failed to launch script build process (CreateProcess failed).");
        return false;
    }

    if (exitCode != 0)
    {
        NOUS_ERROR("Scripts recompilation failed! ExitCode=%lu", (unsigned long)exitCode);

        // Emit a clear, user-facing explanation for environment-setup failures.
        // Compilation errors are already self-explanatory in the captured log above.
        switch (failureCause)
        {
        case BuildFailureCause::VswhereMissing:
            NOUS_ERROR("Cause: Visual Studio Installer is not present on this machine.");
            NOUS_ERROR("       The script hot-reload feature requires Visual Studio 2022 (Community, Professional,");
            NOUS_ERROR("       or Build Tools) with the 'Desktop development with C++' workload installed.");
            NOUS_ERROR("       Download: https://visualstudio.microsoft.com/downloads/");
            NOUS_ERROR("       Expected vswhere.exe at: %%ProgramFiles(x86)%%\\Microsoft Visual Studio\\Installer\\vswhere.exe");
            break;

        case BuildFailureCause::VsCppToolsMissing:
            NOUS_ERROR("Cause: Visual Studio is installed, but the C++ build tools are missing.");
            NOUS_ERROR("       Open the Visual Studio Installer and add the 'Desktop development with C++' workload");
            NOUS_ERROR("       (must include 'MSVC v143 - VS 2022 C++ x64/x86 build tools' and a Windows 10/11 SDK).");
            NOUS_ERROR("       Without it, Scripts.dll cannot be compiled at runtime.");
            break;

        case BuildFailureCause::Vcvars64Missing:
            NOUS_ERROR("Cause: Visual Studio is installed, but 'vcvars64.bat' is missing.");
            NOUS_ERROR("       Your VS installation appears incomplete or corrupted. Repair it via the");
            NOUS_ERROR("       Visual Studio Installer (More -> Repair) and ensure the C++ workload is selected.");
            break;

        case BuildFailureCause::VcvarsInitFailed:
            NOUS_ERROR("Cause: 'vcvars64.bat' was found but failed to initialize the MSVC environment.");
            NOUS_ERROR("       This usually indicates a corrupted Visual Studio installation or a missing");
            NOUS_ERROR("       Windows SDK component. Try repairing VS via the Visual Studio Installer.");
            break;

        case BuildFailureCause::SdkHeadersMissing:
        case BuildFailureCause::SdkEngineApiMissing:
            NOUS_ERROR("Cause: The packaged Script SDK is incomplete (engine deployment issue).");
            NOUS_ERROR("       Verify that the 'EngineCore/Scripts/SDK' folder exists next to the executable");
            NOUS_ERROR("       and contains the SDK headers and 'src/EngineAPI.cpp'.");
            break;

        case BuildFailureCause::ScriptsFolderMissing:
            NOUS_ERROR("Cause: No scripts source folder was found.");
            NOUS_ERROR("       Expected an 'Assets' directory containing one or more .cpp script files.");
            break;

        case BuildFailureCause::CompilationFailed:
            NOUS_ERROR("Cause: One or more user scripts failed to compile. See the cl.exe output above.");
            break;

        case BuildFailureCause::None:
        default:
            NOUS_ERROR("Cause: Unknown. Inspect the captured build output above for details.");
            break;
        }
        return false;
    }

    NOUS_INFO("Scripts recompiled successfully!");
#else
    const std::string shPath = (GetExeDir() / "EngineCore" / "Scripts" / "RebuildScripts.sh").string();
#ifdef _DEBUG
    const std::string cmd = "bash \"" + shPath + "\" Debug 2>&1";
#else
    const std::string cmd = "bash \"" + shPath + "\" Release 2>&1";
#endif

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe)
    {
        NOUS_ERROR("Failed to launch script build process (popen failed).");
        return false;
    }

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe))
    {
        std::string line(buffer);
        if (!line.empty() && line.back() == '\n') line.pop_back();
        if (!line.empty() && line.back() == '\r') line.pop_back();

        if (line.find("error") != std::string::npos || line.find("ERROR") != std::string::npos)
            NOUS_ERROR("[ScriptManager::ReloadScriptLibrary] %s", line.c_str());
        else
            NOUS_INFO("[ScriptManager::ReloadScriptLibrary] %s", line.c_str());
    }

    const int exitCode = pclose(pipe);
    if (exitCode != 0)
    {
        NOUS_ERROR("Scripts recompilation failed! ExitCode=%d", exitCode);
        return false;
    }

    NOUS_INFO("Scripts recompiled successfully!");
#endif

    return LoadScriptLibrary(dllPath);
}


std::vector<std::string> ScriptManager::GetAvailableScriptNames() const
{
    std::vector<std::string> names;
    if (!m_scriptRegistry) return names;
    for (const auto& [name, _] : m_scriptRegistry->GetAll())
        names.push_back(name);
    return names;
}

IScript* ScriptManager::CreateScriptInstance(const std::string& scriptName) {
    if (!m_scriptRegistry) {
        NOUS_ERROR("Script registry not available");
        return nullptr;
    }

    IScript* script = m_scriptRegistry->Create(scriptName);
    if (!script) {
        NOUS_ERROR("Failed to create script instance: %s", scriptName.c_str());
    }
    return script;
}

// Platform-specific implementations
void* ScriptManager::LoadDLL(const std::string& path) {
#ifdef _WIN32
    return LoadLibraryA(path.c_str());
#else
    return dlopen(path.c_str(), RTLD_LAZY | RTLD_LOCAL);
#endif
}

void ScriptManager::UnloadLibrary(void* handle) {
#ifdef _WIN32
    if (handle)
    {
        // Don't try to delete the file here - that's unsafe
        // Let the build system handle file management
        FreeLibrary(static_cast<HMODULE>(handle));
    }
#else
    if (handle) dlclose(handle);
#endif
}

void* ScriptManager::GetSymbol(void* handle, const std::string& symbol) {
#ifdef _WIN32
    return GetProcAddress(static_cast<HMODULE>(handle), symbol.c_str());
#else
    return dlsym(handle, symbol.c_str());
#endif
}

// ---------------------------------------------------------------------------
// CScript component registry
// ---------------------------------------------------------------------------

void ScriptManager::RegisterScriptComponent(CScript* component)
{
    std::lock_guard<std::mutex> lock(m_scriptComponentsMutex);
    m_scriptComponents.push_back(component);
}

void ScriptManager::UnregisterScriptComponent(CScript* component)
{
    std::lock_guard<std::mutex> lock(m_scriptComponentsMutex);
    auto it = std::find(m_scriptComponents.begin(), m_scriptComponents.end(), component);
    if (it != m_scriptComponents.end())
        m_scriptComponents.erase(it);
}

void ScriptManager::DispatchLateUpdate(float dt)
{
    std::lock_guard<std::mutex> lock(m_scriptComponentsMutex);
    for (auto* cs : m_scriptComponents)
        if (cs) cs->LateUpdate(dt);
}

void ScriptManager::DispatchFixedUpdate(float fixedDt)
{
    std::lock_guard<std::mutex> lock(m_scriptComponentsMutex);
    for (auto* cs : m_scriptComponents)
        if (cs) cs->FixedUpdate(fixedDt);
}

void ScriptManager::RecreateAllInstances()
{
    std::lock_guard<std::mutex> lock(m_scriptComponentsMutex);
    for (auto* cs : m_scriptComponents)
        if (cs) cs->RecreateInstances();
}

void ScriptManager::StartAllInstances()
{
    std::lock_guard<std::mutex> lock(m_scriptComponentsMutex);
    for (auto* cs : m_scriptComponents)
        if (cs) cs->StartInstances();
}

void ScriptManager::CleanupScripts()
{
    std::lock_guard<std::mutex> lock(m_scriptComponentsMutex);
    for (auto* cs : m_scriptComponents)
    {
        if (cs)
        {
            cs->ClearInstances();
            cs->ClearRegistrationState();
        }
    }
    m_scriptComponents.clear();
    NOUS_INFO("Cleaned up all CScript instances");
}

// Platform-specific filename of the compiled scripts shared library.
// Matches the build scripts (RebuildScripts.sh emits Scripts.so) and the initial-load
// name in ModuleScene — the hot-reload path must agree or LoadDLL looks for the wrong file.
static constexpr const char* ScriptLibFileName()
{
#ifdef _WIN32
    return "Scripts.dll";
#elif defined(__APPLE__)
    return "Scripts.dylib";
#else
    return "Scripts.so";
#endif
}

void ScriptManager::RecompileScripts()
{
    const std::string dllPath =
        (GetExeDir() / "Library" / "Scripts" / ScriptLibFileName()).string();

    {
        std::lock_guard<std::mutex> lock(m_scriptComponentsMutex);
        for (auto* cs : m_scriptComponents)
            if (cs) cs->ClearInstances();
    }

    if (ReloadScriptLibrary(dllPath))
    {
        std::lock_guard<std::mutex> lock(m_scriptComponentsMutex);
        for (auto* cs : m_scriptComponents)
            if (cs) cs->RecreateInstances();
        NOUS_INFO("Script hot-reload completed successfully");
    }
    else
    {
        NOUS_ERROR("Script hot-reload failed");
    }
}

// ---------------------------------------------------------------------------
// Script generation
// ---------------------------------------------------------------------------

bool ScriptManager::GenerateScript(const std::string& className, const std::string& directory)
{
    const std::string templatePath = (GetExeDir() / "EngineCore" / "Scripts" / "ScriptTemplate.inl").string();
    const std::string outputPath = (std::filesystem::path(directory) / (className + ".cpp")).string();

    // Read the template file
    std::ifstream templateFile(templatePath);
    if (!templateFile.is_open()) {
        return false; // Failed to open template
    }

    // Read entire template content
    std::stringstream buffer;
    buffer << templateFile.rdbuf();
    std::string content = buffer.str();
    templateFile.close();

    // Replace all occurrences of $CLASSNAME$ with the actual class name
    size_t pos = 0;
    while ((pos = content.find("$CLASSNAME$", pos)) != std::string::npos) {
        content.replace(pos, 11, className); // 11 is length of "$CLASSNAME$"
        pos += className.length();
    }

    // Write the modified content to output file
    std::ofstream outputFile(outputPath);
    if (!outputFile.is_open()) {
        return false; // Failed to create output file
    }

    outputFile << content;
    outputFile.close();

    return true;
}
