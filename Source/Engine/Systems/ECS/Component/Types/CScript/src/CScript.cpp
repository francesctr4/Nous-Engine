//
// Created by TheFr on 09/11/2025.
//

#include "Engine/Systems/ECS/Component/Types/CScript/include/CScript.h"
#include "Engine/Scripting/Internal/IScript.inl"   // ScriptProperty, GetProperties()
#include "Engine/Scripting/iScriptRegistry.h"
#include "Engine/Systems/ECS/ComponentServices.h"
#include "Engine/Systems/ECS/Scene/include/iSceneHost.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include <Logger/Logger.h>

#include "Engine/Utils/Serialization/JsonFile/JsonObject.h"
#include "Engine/Utils/Serialization/JsonFile/JsonArray.h"
#include <algorithm>

// ---------------------------------------------------------------------------
// Destructor
// ---------------------------------------------------------------------------

CScript::~CScript()
{
    DestroyInstances();
}

CScript::CScript(const CScript& other)
    : m_scriptNames(other.m_scriptNames)
    , m_savedProperties(other.m_savedProperties)
{
    // m_instances, m_started, m_registered, m_reloading left at defaults —
    // instances are DLL pointers that cannot be shallow-copied.
}

CScript& CScript::operator=(const CScript& other)
{
    if (this != &other)
    {
        DestroyInstances();
        m_scriptNames     = other.m_scriptNames;
        m_savedProperties = other.m_savedProperties;
        m_started         = false;
        m_registered      = false;
        m_reloading.store(false, std::memory_order_relaxed);
    }
    return *this;
}

// ---------------------------------------------------------------------------
// Component lifecycle
// ---------------------------------------------------------------------------

void CScript::OnStart()
{
    // Always register so the component appears in the registry and can be found
    // by PressPlay() / hot-reload regardless of simulation state.
    const ComponentServices& s = Services();
    if (!s.scripts) return;
    s.scripts->RegisterScriptComponent(this);
    m_registered = true;

    // Always create instances — even in edit mode — so the Inspector can
    // display/edit SCRIPT_FIELD values. ApplyProperties() inside CreateInstances()
    // loads any values previously restored from disk or a hot-reload snapshot.
    CreateInstances();

    // Only fire the Awake/Start lifecycle when the simulation is actually running
    // (covers scene loads that happen mid-play).
    if (s.host && !s.host->IsStopped())
        StartInstances();
}

void CScript::OnUpdate(float dt)
{
    if (dt <= 0.0f || m_reloading.load(std::memory_order_seq_cst)) return;
    for (auto* inst : m_instances)
        if (inst) inst->Update(dt);
}

void CScript::LateUpdate(float dt)
{
    if (m_reloading.load(std::memory_order_seq_cst)) return;
    for (auto* inst : m_instances)
        if (inst) inst->LateUpdate(dt);
}

void CScript::FixedUpdate(float fixedDt)
{
    if (m_reloading.load(std::memory_order_seq_cst)) return;
    for (auto* inst : m_instances)
        if (inst) inst->FixedUpdate(fixedDt);
}

void CScript::OnDestroy()
{
    // Unregister only if we actually registered — avoids calling into the scene
    // module if OnStart() was never reached (e.g., component destroyed mid-construction).
    if (m_registered)
    {
        if (IScriptRegistry* scripts = Services().scripts)
            scripts->UnregisterScriptComponent(this);
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

    const ComponentServices& s = Services();
    if (!s.scripts) return;
    IScript* inst = s.scripts->CreateScriptInstance(scriptName);
    if (!inst)
    {
        NOUS_WARN("[CScript] Failed to add script '%s' — not found in registry", scriptName.c_str());
        return;
    }

    inst->SetOwnerID(go.GetID());
    m_instances.push_back(inst);

    // Only invoke the lifecycle when the simulation is live; otherwise the
    // instance just sits waiting for PressPlay (but its fields are editable now).
    if (s.host && !s.host->IsStopped())
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
    m_reloading.store(true, std::memory_order_seq_cst);
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
        // No host wired (headless) counts as "not simulating", matching the old
        // behaviour where an unreachable ModuleScene meant no lifecycle call.
        const ISceneHost* host = Services().host;
        if (!host || host->IsStopped())
        {
            m_reloading.store(false, std::memory_order_seq_cst);
            return;
        }
    }

    StartInstances();
    m_reloading.store(false, std::memory_order_seq_cst);
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
    if (!go.IsValid()) return;

    IScriptRegistry* sm = Services().scripts;
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
                case ScriptProperty::Type::Float:      saved.value.f  = *static_cast<float*>      (prop.ptr); break;
                case ScriptProperty::Type::Int:        saved.value.i  = *static_cast<int32_t*>    (prop.ptr); break;
                case ScriptProperty::Type::Bool:       saved.value.b  = *static_cast<bool*>       (prop.ptr); break;
                case ScriptProperty::Type::GameObject: saved.value.u  = *static_cast<uint32_t*>   (prop.ptr); break;
                case ScriptProperty::Type::String:     saved.strValue = *static_cast<std::string*>(prop.ptr); break;
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
                case ScriptProperty::Type::Float:      *static_cast<float*>      (prop.ptr) = saved.value.f;  break;
                case ScriptProperty::Type::Int:        *static_cast<int32_t*>    (prop.ptr) = saved.value.i;  break;
                case ScriptProperty::Type::Bool:       *static_cast<bool*>       (prop.ptr) = saved.value.b;  break;
                case ScriptProperty::Type::GameObject: *static_cast<uint32_t*>   (prop.ptr) = saved.value.u;  break;
                case ScriptProperty::Type::String:     *static_cast<std::string*>(prop.ptr) = saved.strValue; break;
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

JsonObject CScript::Serialize() const
{
    // Snapshot live property values before serializing (const_cast is safe: we only read via void*)
    const_cast<CScript*>(this)->SaveProperties();

    JsonObject root;
    root.Set("type", "CScript");

    JsonArray scriptsArr;
    for (const auto& name : m_scriptNames)
        scriptsArr.Append(name);
    root.Set("scripts", std::move(scriptsArr));

    // Property values object  { scriptName: { propName: { type, value } } }
    JsonObject propsObj;
    for (const auto& [scriptName, propMap] : m_savedProperties)
    {
        JsonObject scriptObj;
        for (const auto& [propName, saved] : propMap)
        {
            JsonObject pObj;
            pObj.Set("type", static_cast<int>(saved.type));
            switch (static_cast<ScriptProperty::Type>(saved.type))
            {
                case ScriptProperty::Type::Float:      pObj.Set("value", saved.value.f);                        break;
                case ScriptProperty::Type::Int:        pObj.Set("value", saved.value.i);                        break;
                case ScriptProperty::Type::Bool:       pObj.Set("value", saved.value.b);                        break;
                case ScriptProperty::Type::GameObject: pObj.Set("value", static_cast<double>(saved.value.u));   break;
                case ScriptProperty::Type::String:     pObj.Set("value", saved.strValue);                       break;
            }
            scriptObj.Set(propName, std::move(pObj));
        }
        propsObj.Set(scriptName, std::move(scriptObj));
    }
    root.Set("properties", std::move(propsObj));
    return root;
}

void CScript::Deserialize(const JsonObject& obj)
{
    m_scriptNames.clear();
    m_savedProperties.clear();

    // Script names
    JsonArray arr = obj.GetArray("scripts");
    if (!arr.IsEmpty())
    {
        const int count = arr.Count();
        for (int i = 0; i < count; ++i)
        {
            const std::string name = arr.GetString(i);
            if (!name.empty()) m_scriptNames.push_back(name);
        }
    }

    // Property values — load BEFORE CreateInstances() so ApplyProperties() can restore fields.
    JsonObject propsObj = obj.GetObject("properties");
    if (propsObj.IsEmpty()) {
        // No saved properties — still recreate instances with the script names we loaded.
        if (m_registered && !m_scriptNames.empty())
            CreateInstances();
        return;
    }

    for (const auto& scriptName : propsObj.GetKeys())
    {
        JsonObject scriptObj = propsObj.GetObject(scriptName);
        if (scriptObj.IsEmpty()) continue;

        auto& propMap = m_savedProperties[scriptName];

        for (const auto& propName : scriptObj.GetKeys())
        {
            JsonObject pObj = scriptObj.GetObject(propName);
            if (pObj.IsEmpty()) continue;

            SavedProperty saved{};
            saved.type = static_cast<uint8_t>(pObj.GetInt("type"));

            switch (static_cast<ScriptProperty::Type>(saved.type))
            {
                case ScriptProperty::Type::Float:      saved.value.f  = pObj.GetFloat ("value");                        break;
                case ScriptProperty::Type::Int:        saved.value.i  = static_cast<int32_t> (pObj.GetInt("value"));   break;
                case ScriptProperty::Type::Bool:       saved.value.b  = pObj.GetBool  ("value");                        break;
                case ScriptProperty::Type::GameObject: saved.value.u  = static_cast<uint32_t>(pObj.GetDouble("value")); break;
                case ScriptProperty::Type::String:     saved.strValue = pObj.GetString("value");                        break;
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
