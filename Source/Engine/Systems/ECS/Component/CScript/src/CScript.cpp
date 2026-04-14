//
// Created by TheFr on 09/11/2025.
//

#include "Engine/Systems/ECS/Component/CScript/include/CScript.h"
#include "Engine/Scripting/Internal/IScript.inl"   // ScriptProperty, GetProperties()
#include "Engine/Scripting/ScriptManager.h"
#include "Engine/Modules/ModuleScene/include/ModuleScene.h"
#include "Engine/Systems/ECS/Scene/include/Scene.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Core/Logger/Logger.h"

#include <parson.h>
#include <algorithm>

// ---------------------------------------------------------------------------
// Destructor
// ---------------------------------------------------------------------------

CScript::~CScript()
{
    DestroyInstances();
}

// ---------------------------------------------------------------------------
// Component lifecycle
// ---------------------------------------------------------------------------

void CScript::OnStart()
{
    // Always register so the component appears in the registry and can be found
    // by PressPlay() / hot-reload regardless of simulation state.
    auto go = GetGameObject();
    if (!go.IsValid() || !go.GetScene()) return;
    ModuleScene* moduleScene = go.GetScene()->GetModuleScene();
    moduleScene->scriptManager->RegisterScriptComponent(this);
    m_registered = true;

    // Always create instances — even in edit mode — so the Inspector can
    // display/edit SCRIPT_FIELD values. ApplyProperties() inside CreateInstances()
    // loads any values previously restored from disk or a hot-reload snapshot.
    CreateInstances();

    // Only fire the Awake/Start lifecycle when the simulation is actually running
    // (covers scene loads that happen mid-play).
    if (!moduleScene->IsStopped())
        StartInstances();
}

void CScript::OnUpdate(float dt)
{
    if (dt <= 0.0f) return;
    for (auto* inst : m_instances)
        if (inst) inst->Update(dt);
}

void CScript::LateUpdate(float dt)
{
    for (auto* inst : m_instances)
        if (inst) inst->LateUpdate(dt);
}

void CScript::OnDestroy()
{
    // Unregister only if we actually registered — avoids calling into the scene
    // module if OnStart() was never reached (e.g., component destroyed mid-construction).
    if (m_registered)
    {
        auto go = GetGameObject();
        if (go.IsValid() && go.GetScene())
        {
            go.GetScene()->GetModuleScene()->scriptManager->UnregisterScriptComponent(this);
        }
        m_registered = false;
    }

    // Instances now exist in both edit and play modes — always tear them down.
    DestroyInstances();
    m_started = false;
}

// ---------------------------------------------------------------------------
// Runtime script management
// ---------------------------------------------------------------------------

void CScript::AddScript(const std::string& scriptName)
{
    m_scriptNames.push_back(scriptName);

    auto go = GetGameObject();
    if (!go.IsValid()) return;

    ModuleScene* moduleScene = go.GetScene() ? go.GetScene()->GetModuleScene() : nullptr;
    if (!moduleScene) return;
    ScriptManager* sm = moduleScene->scriptManager;
    IScript* inst = sm->CreateScriptInstance(scriptName);
    if (!inst)
    {
        NOUS_WARN("[CScript] Failed to add script '%s' — not found in registry", scriptName.c_str());
        return;
    }

    inst->SetOwnerID(go.GetID());
    m_instances.push_back(inst);

    // Only invoke the lifecycle when the simulation is live; otherwise the
    // instance just sits waiting for PressPlay (but its fields are editable now).
    if (!moduleScene->IsStopped())
    {
        inst->Awake();
        inst->Start();
        NOUS_INFO("[CScript] Added and started script '%s' on '%s'",
                  scriptName.c_str(), go.GetName().c_str());
    }
    else
    {
        NOUS_INFO("[CScript] Added script '%s' on '%s' (will start on Play)",
                  scriptName.c_str(), go.GetName().c_str());
    }
}

void CScript::RemoveScript(const std::string& scriptName)
{
    auto it = std::find(m_scriptNames.begin(), m_scriptNames.end(), scriptName);
    if (it == m_scriptNames.end()) return;

    const size_t idx = static_cast<size_t>(it - m_scriptNames.begin());
    m_scriptNames.erase(it);

    if (idx < m_instances.size())
    {
        IScript* inst = m_instances[idx];
        if (inst)
        {
            if (m_started) inst->OnDestroy();
            inst->Destroy();
        }
        m_instances.erase(m_instances.begin() + static_cast<ptrdiff_t>(idx));
    }
}

// ---------------------------------------------------------------------------
// Hot-reload helpers (called by ModuleScene)
// ---------------------------------------------------------------------------

void CScript::ClearInstances()
{
    SaveProperties();   // snapshot values before the DLL instances are destroyed
    DestroyInstances();
    m_started = false;
}

void CScript::RecreateInstances()
{
    CreateInstances();

    // Only re-enter the lifecycle when the simulation is live; in edit mode
    // we just want the fresh instances available for Inspector editing.
    {
        auto go = GetGameObject();
        if (!go.IsValid() || !go.GetScene() || go.GetScene()->GetModuleScene()->IsStopped())
            return;
    }

    StartInstances();
}

void CScript::StartInstances()
{
    if (m_started) return;

    for (auto* inst : m_instances)
    {
        if (inst)
        {
            inst->Awake();
            inst->Start();
        }
    }
    m_started = true;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void CScript::CreateInstances()
{
    DestroyInstances();

    auto go = GetGameObject();
    if (!go.IsValid() || !go.GetScene() || !go.GetScene()->GetModuleScene()) return;
    ScriptManager* sm = go.GetScene()->GetModuleScene()->scriptManager;
    if (!sm) return;

    const uint32_t ownerID  = go.GetID();
    const std::string goName = go.GetName();

    for (const auto& name : m_scriptNames)
    {
        IScript* inst = sm->CreateScriptInstance(name);
        if (inst)
        {
            inst->SetOwnerID(ownerID);

            m_instances.push_back(inst);
            NOUS_INFO("[CScript] Created instance of '%s' on '%s'",
                      name.c_str(), goName.c_str());
        }
        else
        {
            NOUS_WARN("[CScript] Script '%s' not found in registry", name.c_str());
        }
    }

    // Restore any previously saved property values (from hot-reload or deserialization)
    ApplyProperties();
}

void CScript::SaveProperties()
{
    NOUS_DEBUG("[CScript::SaveProperties] this=%p owner=%u instances=%zu names=%zu",
               static_cast<const void*>(this),
               GetGameObject().IsValid() ? GetGameObject().GetID() : 0u,
               m_instances.size(), m_scriptNames.size());

    if (m_instances.size() != m_scriptNames.size())
    {
        NOUS_ERROR("[CScript::SaveProperties] size mismatch — skipping to avoid crash");
        return;
    }

    for (size_t i = 0; i < m_instances.size(); ++i)
    {
        IScript* inst = m_instances[i];
        NOUS_DEBUG("[CScript::SaveProperties]   [%zu] inst=%p name='%s'",
                   i, static_cast<const void*>(inst), m_scriptNames[i].c_str());
        if (!inst) continue;

        const std::string& scriptName = m_scriptNames[i];
        auto& propMap = m_savedProperties[scriptName];

        for (const auto& prop : inst->GetProperties())
        {
            SavedProperty saved{};
            saved.type = static_cast<uint8_t>(prop.type);

            switch (prop.type)
            {
                case ScriptProperty::Type::Float:      saved.value.f = *static_cast<float*>   (prop.ptr); break;
                case ScriptProperty::Type::Int:        saved.value.i = *static_cast<int32_t*> (prop.ptr); break;
                case ScriptProperty::Type::Bool:       saved.value.b = *static_cast<bool*>    (prop.ptr); break;
                case ScriptProperty::Type::GameObject: saved.value.u = *static_cast<uint32_t*>(prop.ptr); break;
            }

            propMap[prop.name] = saved;
        }
    }
}

void CScript::ApplyProperties()
{
    for (size_t i = 0; i < m_instances.size(); ++i)
    {
        IScript* inst = m_instances[i];
        if (!inst) continue;

        const std::string& scriptName = m_scriptNames[i];
        auto it = m_savedProperties.find(scriptName);
        if (it == m_savedProperties.end()) continue;

        for (const auto& prop : inst->GetProperties())
        {
            auto valIt = it->second.find(prop.name);
            if (valIt == it->second.end()) continue;

            const SavedProperty& saved = valIt->second;
            if (saved.type != static_cast<uint8_t>(prop.type)) continue;

            switch (prop.type)
            {
                case ScriptProperty::Type::Float:      *static_cast<float*>   (prop.ptr) = saved.value.f; break;
                case ScriptProperty::Type::Int:        *static_cast<int32_t*> (prop.ptr) = saved.value.i; break;
                case ScriptProperty::Type::Bool:       *static_cast<bool*>    (prop.ptr) = saved.value.b; break;
                case ScriptProperty::Type::GameObject: *static_cast<uint32_t*>(prop.ptr) = saved.value.u; break;
            }
        }
    }
}

void CScript::DestroyInstances()
{
    for (auto* inst : m_instances)
        if (inst) inst->Destroy();

    m_instances.clear();
}

// ---------------------------------------------------------------------------
// Serialization
// ---------------------------------------------------------------------------

JSON_Value* CScript::Serialize() const
{
    // Snapshot live property values before serializing (const_cast is safe: we only read via void*)
    const_cast<CScript*>(this)->SaveProperties();

    JSON_Value*  val = json_value_init_object();
    JSON_Object* obj = json_value_get_object(val);

    json_object_set_string(obj, "type", "CScript");

    // Script names array
    JSON_Value* arrVal = json_value_init_array();
    JSON_Array* arr    = json_value_get_array(arrVal);
    for (const auto& name : m_scriptNames)
        json_array_append_string(arr, name.c_str());
    json_object_set_value(obj, "scripts", arrVal);

    // Property values object  { scriptName: { propName: { type, value } } }
    JSON_Value*  propsVal = json_value_init_object();
    JSON_Object* propsObj = json_value_get_object(propsVal);

    for (const auto& [scriptName, propMap] : m_savedProperties)
    {
        JSON_Value*  scriptVal = json_value_init_object();
        JSON_Object* scriptObj = json_value_get_object(scriptVal);

        for (const auto& [propName, saved] : propMap)
        {
            JSON_Value*  pVal = json_value_init_object();
            JSON_Object* pObj = json_value_get_object(pVal);

            json_object_set_number(pObj, "type", static_cast<double>(saved.type));

            switch (static_cast<ScriptProperty::Type>(saved.type))
            {
                case ScriptProperty::Type::Float:      json_object_set_number (pObj, "value", saved.value.f); break;
                case ScriptProperty::Type::Int:        json_object_set_number (pObj, "value", saved.value.i); break;
                case ScriptProperty::Type::Bool:       json_object_set_boolean(pObj, "value", saved.value.b); break;
                case ScriptProperty::Type::GameObject: json_object_set_number (pObj, "value", saved.value.u); break;
            }

            json_object_set_value(scriptObj, propName.c_str(), pVal);
        }

        json_object_set_value(propsObj, scriptName.c_str(), scriptVal);
    }

    json_object_set_value(obj, "properties", propsVal);
    return val;
}

void CScript::Deserialize(JSON_Object* obj)
{
    m_scriptNames.clear();
    m_savedProperties.clear();

    // Script names
    JSON_Array* arr = json_object_get_array(obj, "scripts");
    if (arr)
    {
        const size_t count = json_array_get_count(arr);
        for (size_t i = 0; i < count; ++i)
        {
            const char* name = json_array_get_string(arr, i);
            if (name) m_scriptNames.emplace_back(name);
        }
    }

    // Property values — load BEFORE CreateInstances() so ApplyProperties() can restore fields.
    JSON_Object* propsObj = json_object_get_object(obj, "properties");
    if (!propsObj) {
        // No saved properties — still recreate instances with the script names we loaded.
        if (m_registered && !m_scriptNames.empty())
            CreateInstances();
        return;
    }

    for (size_t si = 0; si < json_object_get_count(propsObj); ++si)
    {
        const char*  scriptName = json_object_get_name(propsObj, si);
        JSON_Object* scriptObj  = json_object_get_object(propsObj, scriptName);
        if (!scriptName || !scriptObj) continue;

        auto& propMap = m_savedProperties[scriptName];

        for (size_t pi = 0; pi < json_object_get_count(scriptObj); ++pi)
        {
            const char*  propName = json_object_get_name(scriptObj, pi);
            JSON_Object* pObj     = json_object_get_object(scriptObj, propName);
            if (!propName || !pObj) continue;

            SavedProperty saved{};
            saved.type = static_cast<uint8_t>(json_object_get_number(pObj, "type"));

            switch (static_cast<ScriptProperty::Type>(saved.type))
            {
                case ScriptProperty::Type::Float:      saved.value.f = static_cast<float>   (json_object_get_number (pObj, "value")); break;
                case ScriptProperty::Type::Int:        saved.value.i = static_cast<int32_t> (json_object_get_number (pObj, "value")); break;
                case ScriptProperty::Type::Bool:       saved.value.b = json_object_get_boolean(pObj, "value") != 0;                   break;
                case ScriptProperty::Type::GameObject: saved.value.u = static_cast<uint32_t>(json_object_get_number (pObj, "value")); break;
            }

            propMap[propName] = saved;
        }
    }

    // Recreate instances now that both script names and saved properties are loaded.
    // OnStart() already ran (m_registered=true) but called CreateInstances() against an
    // empty m_scriptNames; this call creates the real instances and ApplyProperties()
    // restores all SCRIPT_FIELD values (including SCRIPT_GAMEOBJECT references).
    if (m_registered && !m_scriptNames.empty())
        CreateInstances();

    NOUS_DEBUG("[CScript::Deserialize] this=%p owner=%u instances=%zu names=%zu",
               static_cast<const void*>(this),
               GetGameObject().IsValid() ? GetGameObject().GetID() : 0u,
               m_instances.size(), m_scriptNames.size());
    for (size_t i = 0; i < m_instances.size(); ++i)
        NOUS_DEBUG("[CScript::Deserialize]   [%zu] inst=%p name='%s'",
                   i, static_cast<const void*>(m_instances[i]),
                   (i < m_scriptNames.size() ? m_scriptNames[i].c_str() : "?"));
}
