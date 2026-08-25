#ifndef NOUS_ENGINE_SCRIPTMANAGER_H
#define NOUS_ENGINE_SCRIPTMANAGER_H

#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include <mutex>

#include "Engine/EngineExport.h"
#include "Engine/Utils/DataStructures/NOUS_Vector.h"
#include "Engine/Scripting/iScriptRegistry.h"

class IScript;
class CScript;
struct EngineAPI;
class ScriptRegistry;
class IScriptInput;
class IScriptSceneHost;

class ScriptManager : public IScriptRegistry
{
public:

    ScriptManager(IScriptInput* input, IScriptSceneHost* sceneHost);
    ~ScriptManager();

    // Hot-reload functionality
    bool LoadScriptLibrary(const std::string& dllPath);
    void UnloadScriptLibrary();
    bool ReloadScriptLibrary(const std::string& dllPath);

    // Recompiles Scripts.dll and hot-reloads all live CScript instances.
    NOUS_ENGINE_API void RecompileScripts();

    // Script management
    IScript* CreateScriptInstance(const std::string& scriptName) override;

    // Returns names of all scripts registered in the loaded DLL (empty if no DLL loaded)
    NOUS_ENGINE_API std::vector<std::string> GetAvailableScriptNames() const;

    // Script generation
    NOUS_ENGINE_API static bool GenerateScript(const std::string& className,
                                                const std::string& directory = "Assets/Scripts");

    // ---------------------------------------------------------------------------
    // CScript component registry — called by CScript::OnStart / OnDestroy
    // ---------------------------------------------------------------------------
    void RegisterScriptComponent(CScript* component) override;
    void UnregisterScriptComponent(CScript* component) override;

    // Dispatches IScript::LateUpdate to all live components. Called from ModuleScene::PostUpdate.
    void DispatchLateUpdate(float dt);

    // Dispatches IScript::FixedUpdate to all live components. Call from a fixed-timestep tick.
    NOUS_ENGINE_API void DispatchFixedUpdate(float fixedDt);

    // Calls RecreateInstances() on every registered component. Called during script hot-reload.
    void RecreateAllInstances();

    // Calls StartInstances() on every registered component (Awake/Start).
    // Called from ModuleScene::PressPlay — instances already exist from edit mode.
    void StartAllInstances();

    // Destroys all DLL instances and clears the registry. Called from ModuleScene::CleanUp.
    void CleanupScripts();

private:

    // Platform-specific library handling
    void* LoadDLL(const std::string& path);
    void* GetSymbol(void* handle, const std::string& symbol);
    void UnloadLibrary(void* handle);

    void* m_libraryHandle;
    std::string m_shadowDllPath;
    ScriptRegistry* m_scriptRegistry;
    EngineAPI* api = nullptr;

    IScriptInput*     m_input;
    IScriptSceneHost* m_sceneHost;

    // Flat registry of every live CScript component in the scene.
    // Used for hot-reload, LateUpdate dispatch, and cleanup.
    NOUS_Vector<CScript*> m_scriptComponents;
    std::mutex            m_scriptComponentsMutex;

};

#endif //NOUS_ENGINE_SCRIPTMANAGER_H
