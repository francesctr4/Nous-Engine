#pragma once

#include <ECS/Component/Component.h>
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>
#include <map>

#include <EngineCore/EngineExport.h>

class IScript;

// Mirrors ScriptProperty::Type without requiring IScript.inl in this header.
// Values must stay in sync with ScriptProperty::Type in IScript.inl.
struct SavedProperty
{
    uint8_t type;   // 0 = Float, 1 = Int, 2 = Bool, 3 = GameObject (uint32_t ID), 4 = String
    union { float f; int32_t i; bool b; uint32_t u; } value{};
    std::string strValue; // used when type == 4 (String)
};

// scriptName → propertyName → saved value
using ScriptPropertyMap = std::map<std::string, std::map<std::string, SavedProperty>>;

class CScript : public Component {
public:
    COMPONENT_TYPE(CScript)

    // CScript must be pointer-stable in the registry: ScriptManager stores raw CScript*
    // pointers, and each CScript owns a vector of IScript* into DLL memory. EnTT's default
    // swap-and-pop storage would move components on entity destruction, silently invalidating
    // those pointers (observed as a crash in SaveProperties when a prefab child was destroyed
    // and refreshed during Play→Stop→Play). Opting into in_place_delete switches CScript to
    // tombstone-based deletion, keeping addresses stable.
    static constexpr bool in_place_delete = true;

    CScript() = default;
    NOUS_ENGINE_API ~CScript() override;

    // Explicitly defined because std::atomic deletes the implicit copy operations.
    // Only script names and saved properties transfer; instances must be recreated via OnStart.
    NOUS_ENGINE_API CScript(const CScript& other);
    NOUS_ENGINE_API CScript& operator=(const CScript& other);

    // Component lifecycle — called by the ECS
    NOUS_ENGINE_API void OnStart()              override;
    NOUS_ENGINE_API void OnUpdate(float dt)     override;
    NOUS_ENGINE_API void OnDestroy()            override;

    // Called by ModuleScene::PostUpdate for the LateUpdate pass
    void LateUpdate(float dt);

    // Called by ScriptManager::DispatchFixedUpdate at a fixed timestep
    void FixedUpdate(float fixedDt);

    // Runtime script attachment (safe to call before or after OnStart)
    NOUS_ENGINE_API void AddScript(const std::string& scriptName);
    NOUS_ENGINE_API void RemoveScript(const std::string& scriptName);

    const std::vector<std::string>& GetScriptNames() const { return m_scriptNames; }

    // Returns the live IScript instance at index i, or nullptr if out of range / not started
    IScript* GetInstance(size_t i) const
    {
        return (i < m_instances.size()) ? m_instances[i] : nullptr;
    }

    // Called by ModuleScene during hot-reload (destroys DLL instances, keeps names intact)
    void ClearInstances();

    // Called by ModuleScene during hot-reload (recreates instances; Awake/Start only if simulation is live)
    void RecreateInstances();

    // Calls Awake/Start on existing instances. Called by ModuleScene::PressPlay.
    // Safe to call multiple times (no-op if already started).
    // Exported: ScriptManager reaches it from inside the DLL, but t_CScript drives
    // it across the DLL boundary to reproduce the editor's load-then-Play ordering.
    NOUS_ENGINE_API void StartInstances();

    // Start instances that were created after OnStart() already ran — i.e. a scene
    // deserialized into a live simulation. No-op while stopped.
    void StartInstancesIfSimulating();

    // Called by ModuleScene::CleanupScripts() during engine shutdown — marks the component
    // as unregistered so that the subsequent OnDestroy() (from scene destruction) does not
    // attempt to call back into the scene module while it is being torn down.
    void ClearRegistrationState() { m_registered = false; }

    // Serialization
    NOUS_ENGINE_API JsonObject Serialize()                   const override;
    NOUS_ENGINE_API void       Deserialize(const JsonObject& obj)  override;

private:
    void CreateInstances();
    void DestroyInstances();
    void SaveProperties();          // snapshot live property values → m_savedProperties
    void ApplyProperties();         // write m_savedProperties back into live instances

    std::vector<std::string>  m_scriptNames;
    std::vector<IScript*>     m_instances;
    bool                      m_started    = false;  // true when DLL instances are alive and ticking
    bool                      m_registered = false;  // true when RegisterScriptComponent has been called
    ScriptPropertyMap         m_savedProperties; // persists across hot-reload and serialization

    // Set to true on the worker thread before instances are destroyed, false after they are
    // recreated. OnUpdate/LateUpdate on the main thread skip iteration while this is set,
    // preventing a use-after-free when hot-reload runs during play mode.
    std::atomic<bool>         m_reloading{false};
};
