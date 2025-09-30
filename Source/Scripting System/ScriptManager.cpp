#include "Scripting System/ScriptManager.h"
#include "Scripting System/Internal/ScriptRegistry.inl"
#include "Utils/Logger.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

ScriptManager::ScriptManager() : m_libraryHandle(nullptr), m_scriptRegistry(nullptr)
{

}

ScriptManager::~ScriptManager() {
    UnloadScriptLibrary();
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

    NOUS_INFO("Script library loaded successfully: %s", dllPath.c_str());
    return true;
}

void ScriptManager::UnloadScriptLibrary() {
    if (m_libraryHandle) {
        UnloadLibrary(m_libraryHandle);
        m_libraryHandle = nullptr;
        m_scriptRegistry = nullptr;
    }
}

bool ScriptManager::ReloadScriptLibrary(const std::string& dllPath) {
    NOUS_INFO("Reloading script library: %s", dllPath.c_str());
    return LoadScriptLibrary(dllPath);
}

IScript* ScriptManager::CreateScriptInstance(const std::string& scriptName) {
    if (!m_scriptRegistry) {
        NOUS_ERROR("Script registry not available");
        return nullptr;
    }

    return m_scriptRegistry->Create(scriptName);
}

const std::unordered_map<std::string, ScriptRegistry::Factory>&
ScriptManager::GetAvailableScripts() const {
    static std::unordered_map<std::string, ScriptRegistry::Factory> empty;
    return m_scriptRegistry ? m_scriptRegistry->GetAll() : empty;
}

void ScriptManager::BuildScriptsDLL()
{
    // TODO: Need to test this properly on the engine
    // Change working directory to your build folder
    int result = std::system(
            "cmake --build \".\" --target Scripts --config Debug-Windows");

    if (result != 0)
        std::cerr << "Failed to build Scripts DLL!" << std::endl;
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
    if (handle) FreeLibrary(static_cast<HMODULE>(handle));
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