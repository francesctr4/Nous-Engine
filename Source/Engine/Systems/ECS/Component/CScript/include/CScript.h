#ifndef NOUS_ENGINE_CSCRIPT_H
#define NOUS_ENGINE_CSCRIPT_H

#include "Engine/Systems/ECS/Component/Component.h"
#include <string>
#include <vector>

#include "Engine/EngineExport.h"

class IScript;

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

    // Called by ModuleScene during hot-reload (destroys DLL instances, keeps names intact)
    void ClearInstances();

    // Called by ModuleScene during hot-reload (recreates instances + Awake + Start, no re-registration)
    void RecreateInstances();

    // Serialization
    NOUS_ENGINE_API JSON_Value* Serialize()             const override;
    NOUS_ENGINE_API void        Deserialize(JSON_Object* obj) override;

private:
    void CreateInstances();
    void DestroyInstances();

    std::vector<std::string> m_scriptNames;
    std::vector<IScript*>    m_instances;
    bool                     m_started = false;
};

#endif // NOUS_ENGINE_CSCRIPT_H
