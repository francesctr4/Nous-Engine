#include <Engine/Scripting/ScriptManager.h>
#include <Engine/Scripting/Internal/ScriptRegistry.inl>
#include "Engine/Core/Logger/Logger.h"
#include <Engine/Core/MemoryManager/MemoryManager.h>
#include "Engine/NOUS_Multithreading/NOUS_Multithreading.h"
#include "Engine/NOUS_Multithreading/NOUS_Thread/include/NOUS_Thread.h"
#include <Engine/Scripting/EngineAPI/EngineAPI.h>
#include <Engine/Scripting/EngineAPI/Bindings/ScriptBindings.h>
#include "Engine/Systems/ECS/Component/CScript/include/CScript.h"

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

ScriptManager::ScriptManager(ModuleInput* moduleInput, ModuleScene* moduleScene)
    : m_libraryHandle(nullptr), m_scriptRegistry(nullptr),
      m_moduleInput(moduleInput), m_moduleScene(moduleScene),
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

    m_libraryHandle = LoadDLL(dllPath);
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

    ScriptBindings::SetupAllBindings(*api, m_moduleInput, m_moduleScene);

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
        // Clear registry before unloading
        m_scriptRegistry = nullptr;

        UnloadLibrary(m_libraryHandle);
        m_libraryHandle = nullptr;

        // Small delay to ensure DLL is fully unloaded
        NOUS_Multithreading::NOUS_Thread::SleepMS(100);
    }
}

bool ScriptManager::ReloadScriptLibrary(const std::string& dllPath)
{
    NOUS_INFO("Reloading script library...");
    UnloadScriptLibrary();

#ifdef _WIN32
    const std::wstring batPath = (GetExeDir() / "Library" / "Scripts" / "RebuildScripts.bat").wstring();
    std::wstring cmd;
#ifdef _DEBUG
    cmd = batPath + L" Debug";
#else
    cmd = batPath + L" Release";
#endif

    DWORD exitCode = 0;
    bool ran = RunProcessCaptureLive(cmd, exitCode, [](const std::string& line)
    {
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
        return false;
    }

    NOUS_INFO("Scripts recompiled successfully!");
#else
    const std::string shPath = (GetExeDir() / "Library" / "Scripts" / "RebuildScripts.sh").string();
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

    if (!WaitForDLLUnload(dllPath)) {
        NOUS_ERROR("DLL is still locked, cannot reload");
        return false;
    }

    return LoadScriptLibrary(dllPath);
}

bool ScriptManager::WaitForDLLUnload(const std::string& dllPath, int maxRetries) {
#ifdef _WIN32
    for (int i = 0; i < maxRetries; ++i) {
        HANDLE fileHandle = CreateFileA(
                dllPath.c_str(),
                GENERIC_READ,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                NULL,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                NULL
        );

        if (fileHandle != INVALID_HANDLE_VALUE) {
            CloseHandle(fileHandle);
            return true; // File is accessible
        }

        DWORD error = GetLastError();
        if (error == ERROR_SHARING_VIOLATION) {
            // DLL is still locked, wait and retry
            NOUS_Multithreading::NOUS_Thread::SleepMS(50);
        } else if (error == ERROR_FILE_NOT_FOUND) {
            // DLL doesn't exist yet (might be compiling)
            NOUS_Multithreading::NOUS_Thread::SleepMS(100);
        } else {
            break; // Other error
        }
    }
#endif
    return true; // On non-Windows or if we can't check, just proceed
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

void ScriptManager::RecreateAllInstances()
{
    std::lock_guard<std::mutex> lock(m_scriptComponentsMutex);
    for (auto* cs : m_scriptComponents)
        if (cs) cs->RecreateInstances();
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

void ScriptManager::RecompileScripts()
{
    const std::string dllPath =
        (GetExeDir() / "Library" / "Scripts" / "Scripts.dll").string();

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

bool ScriptManager::GenerateScript(const std::string& className)
{
    const std::string templatePath = (GetExeDir() / "Library" / "Scripts" / "ScriptTemplate.inl").string();
    const std::string& outputPath = "Assets/Scripts/" + className + ".cpp";

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
