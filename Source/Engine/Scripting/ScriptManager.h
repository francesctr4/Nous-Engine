#ifndef NOUS_ENGINE_SCRIPTMANAGER_H
#define NOUS_ENGINE_SCRIPTMANAGER_H

#include <string>
#include <memory>
#include <unordered_map>
#include <vector>

#include "Engine/EngineExport.h"

class IScript;
struct EngineAPI;
class ScriptRegistry;
class ModuleInput;
class ModuleScene;

class ScriptManager
{
public:

    ScriptManager(ModuleInput* moduleInput, ModuleScene* moduleScene);
    ~ScriptManager();

    // Hot-reload functionality
    bool LoadScriptLibrary(const std::string& dllPath);
    void UnloadScriptLibrary();
    bool ReloadScriptLibrary(const std::string& dllPath);

    // Script management
    IScript* CreateScriptInstance(const std::string& scriptName);

    // Returns names of all scripts registered in the loaded DLL (empty if no DLL loaded)
    NOUS_ENGINE_API std::vector<std::string> GetAvailableScriptNames() const;

    // Script generation
    NOUS_ENGINE_API static bool GenerateScript(const std::string& className);

private:

    // Platform-specific library handling
    void* LoadDLL(const std::string& path);
    void* GetSymbol(void* handle, const std::string& symbol);
    bool WaitForDLLUnload(const std::string& dllPath, int maxRetries = 10);
    void UnloadLibrary(void* handle);

    void* m_libraryHandle;
    ScriptRegistry* m_scriptRegistry;
    EngineAPI* api;

    ModuleInput* m_moduleInput;
    ModuleScene* m_moduleScene;

};

#endif //NOUS_ENGINE_SCRIPTMANAGER_H
