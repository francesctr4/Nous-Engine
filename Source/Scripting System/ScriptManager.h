#ifndef NOUS_ENGINE_SCRIPTMANAGER_H
#define NOUS_ENGINE_SCRIPTMANAGER_H

#include <string>
#include <memory>
#include <unordered_map>

#include "Scripting System/Internal/ScriptRegistry.inl"

class IScript;
struct EngineAPI;

class ScriptManager
{
public:

    ScriptManager();
    ~ScriptManager();

    // Hot-reload functionality
    bool LoadScriptLibrary(const std::string& dllPath);
    void UnloadScriptLibrary();
    bool ReloadScriptLibrary(const std::string& dllPath);

    // Script management
    IScript* CreateScriptInstance(const std::string& scriptName);
    const std::unordered_map<std::string, ScriptRegistry::Factory>& GetAvailableScripts() const;

    // Check if library is loaded
    bool IsLibraryLoaded() const { return m_libraryHandle != nullptr; }

private:

    void* m_libraryHandle;
    ScriptRegistry* m_scriptRegistry;
    EngineAPI* api;

    // Platform-specific library handling
    void* LoadDLL(const std::string& path);
    void UnloadLibrary(void* handle);
    void* GetSymbol(void* handle, const std::string& symbol);

    void BuildScriptsDLL();
};

#endif //NOUS_ENGINE_SCRIPTMANAGER_H
