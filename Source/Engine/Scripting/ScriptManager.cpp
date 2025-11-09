#include <Engine/Scripting/ScriptManager.h>
#include <Engine/Scripting/Internal/ScriptRegistry.inl>
#include "Engine/Core/Logger/Logger.h"
#include <Engine/Core/MemoryManager/MemoryManager.h>
#include "Engine/NOUS_Multithreading/NOUS_Multithreading.h"
#include "Engine/NOUS_Multithreading/NOUS_Thread/include/NOUS_Thread.h"
#include <Engine/Scripting/EngineAPI/EngineAPI.h>
#include <Engine/Scripting/EngineAPI/Bindings/ScriptBindings.h>

#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

ScriptManager::ScriptManager() : m_libraryHandle(nullptr), m_scriptRegistry(nullptr)
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

    ScriptBindings::SetupAllBindings(*api);

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

    // Unload current library first
    UnloadScriptLibrary();

    // Build the scripts
    int result = std::system("Scripts\\RebuildScripts.bat");

    if (result == 0) {
        NOUS_INFO("Scripts recompiled successfully!");
    } else {
        NOUS_ERROR("Scripts recompilation failed!");
        return false;
    }

    // Wait for file system and ensure DLL can be loaded
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

bool ScriptManager::GenerateScript(const std::string& className)
{
    const std::string& templatePath = "Scripts/ScriptTemplate.inl";
    const std::string& outputPath = "../../../Assets/Scripts/" + className + ".cpp";

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
