//
// Created by TheFr on 09/11/2025.
//

#include "Engine/Systems/ECS/Component/CScript/include/CScript.h"
#include "Engine/Scripting/Internal/IScript.inl"
#include "Engine/Scripting/ScriptManager.h"
#include "Engine/Core/Application.h"
#include "Engine/Modules/ModuleScene/include/ModuleScene.h"
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
    CreateInstances();
    m_started = true;

    External->scene->RegisterScriptComponent(this);

    for (auto* inst : m_instances)
    {
        if (inst)
        {
            inst->Awake();
            inst->Start();
        }
    }
}

void CScript::OnUpdate(float dt)
{
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
    if (m_started)
    {
        External->scene->UnregisterScriptComponent(this);
        DestroyInstances();
        m_started = false;
    }
}

// ---------------------------------------------------------------------------
// Runtime script management
// ---------------------------------------------------------------------------

void CScript::AddScript(const std::string& scriptName)
{
    m_scriptNames.push_back(scriptName);

    if (m_started && m_GameObject)
    {
        ScriptManager* sm = External->scene->scriptManager;
        IScript* inst = sm->CreateScriptInstance(scriptName);
        if (inst)
        {
            inst->SetOwnerID(m_GameObject->GetID());
            m_instances.push_back(inst);
            inst->Awake();
            inst->Start();
            NOUS_INFO("[CScript] Added and started script '%s' on '%s'",
                      scriptName.c_str(), m_GameObject->GetName().c_str());
        }
        else
        {
            NOUS_WARN("[CScript] Failed to add script '%s' — not found in registry", scriptName.c_str());
        }
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
            inst->OnDestroy();
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
    DestroyInstances();
    m_started = false;
}

void CScript::RecreateInstances()
{
    CreateInstances();
    m_started = true;

    for (auto* inst : m_instances)
    {
        if (inst)
        {
            inst->Awake();
            inst->Start();
        }
    }
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void CScript::CreateInstances()
{
    DestroyInstances();

    if (!External || !External->scene || !External->scene->scriptManager) return;

    ScriptManager* sm = External->scene->scriptManager;

    for (const auto& name : m_scriptNames)
    {
        IScript* inst = sm->CreateScriptInstance(name);
        if (inst)
        {
            if (m_GameObject)
                inst->SetOwnerID(m_GameObject->GetID());

            m_instances.push_back(inst);
            NOUS_INFO("[CScript] Created instance of '%s' on '%s'",
                      name.c_str(),
                      m_GameObject ? m_GameObject->GetName().c_str() : "unknown");
        }
        else
        {
            NOUS_WARN("[CScript] Script '%s' not found in registry", name.c_str());
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
    JSON_Value*  val = json_value_init_object();
    JSON_Object* obj = json_value_get_object(val);

    json_object_set_string(obj, "type", "CScript");

    JSON_Value* arrVal = json_value_init_array();
    JSON_Array* arr    = json_value_get_array(arrVal);

    for (const auto& name : m_scriptNames)
        json_array_append_string(arr, name.c_str());

    json_object_set_value(obj, "scripts", arrVal);
    return val;
}

void CScript::Deserialize(JSON_Object* obj)
{
    m_scriptNames.clear();

    JSON_Array* arr = json_object_get_array(obj, "scripts");
    if (!arr) return;

    const size_t count = json_array_get_count(arr);
    for (size_t i = 0; i < count; ++i)
    {
        const char* name = json_array_get_string(arr, i);
        if (name) m_scriptNames.push_back(name);
    }
}
