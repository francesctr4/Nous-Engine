#ifndef NOUS_ENGINE_CSCRIPT_H
#define NOUS_ENGINE_CSCRIPT_H

#include "Engine/Systems/ECS/Component/Component.h"
#include <cstdint>
#include <string>
#include <vector>
#include <map>

#include "Engine/EngineExport.h"

class IScript;

// Mirrors ScriptProperty::Type without requiring IScript.inl in this header.
// Values must stay in sync with ScriptProperty::Type in IScript.inl.
struct SavedProperty
{
    uint8_t type;   // 0 = Float, 1 = Int, 2 = Bool, 3 = GameObject (uint32_t ID)
    union { float f; int32_t i; bool b; uint32_t u; } value{};
};

// scriptName → propertyName → saved value
using ScriptPropertyMap = std::map<std::string, std::map<std::string, SavedProperty>>;

class CScript : public Component {
public:
    COMPONENT_TYPE(CScript)

    NOUS_ENGINE_API ~CScript() override;

    // Component lifecycle — called by the ECS
    NOUS_ENGINE_API void OnStart()              override;
    NOUS_ENGINE_API void OnUpdate(float dt)     override;
    NOUS_ENGINE_API void OnDestroy()            override;

    // Called by ModuleScene::PostUpdate for the LateUpdate pass
    void LateUpdate(float dt);

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

    // Called by ModuleScene during hot-reload (recreates instances + Awake + Start, no re-registration)
    void RecreateInstances();

    // Called by ModuleScene::CleanupScripts() during engine shutdown — marks the component
    // as unregistered so that the subsequent OnDestroy() (from scene destruction) does not
    // attempt to call back into the scene module while it is being torn down.
    void ClearRegistrationState() { m_registered = false; }

    // Serialization
    NOUS_ENGINE_API JSON_Value* Serialize()             const override;
    NOUS_ENGINE_API void        Deserialize(JSON_Object* obj) override;

private:
    void CreateInstances();
    void DestroyInstances();
    void SaveProperties();          // snapshot live property values → m_savedProperties
    void ApplyProperties();         // write m_savedProperties back into live instances

    std::vector<std::string> m_scriptNames;
    std::vector<IScript*>    m_instances;
    bool                     m_started    = false;  // true when DLL instances are alive and ticking
    bool                     m_registered = false;  // true when RegisterScriptComponent has been called
    ScriptPropertyMap        m_savedProperties; // persists across hot-reload and serialization
};

#endif // NOUS_ENGINE_CSCRIPT_H
