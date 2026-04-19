#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/ECS/Scene/include/Scene.h"
#include "Engine/Systems/ECS/ECSInternalComponents.h"

#include "Engine/Systems/ECS/Component/CTransform/include/CTransform.h"
#include "Engine/Systems/ECS/Component/CMesh/include/CMesh.h"
#include "Engine/Systems/ECS/Component/CMaterial/include/CMaterial.h"
#include "Engine/Systems/ECS/Component/CCamera/include/CCamera.h"
#include "Engine/Systems/ECS/Component/CLight/include/CLight.h"
#include "Engine/Systems/ECS/Component/CScript/include/CScript.h"
#include "Engine/Systems/ECS/Component/CPrefab/include/CPrefab.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/Core/Logger/Logger.h"

#include <parson.h>
#include <ranges>
#include <algorithm>
#include <cstring>

// ── Identity ──────────────────────────────────────────────────────────────────

uint32_t GameObject::GetID() const {
    if (!m_Registry) return 0;
    if (const auto* info = m_Registry->try_get<CEntityInfo>(m_Entity))
        return info->id;
    return 0;
}

const std::string& GameObject::GetName() const {
    static const std::string c_empty;
    if (!m_Registry) return c_empty;
    if (const auto* info = m_Registry->try_get<CEntityInfo>(m_Entity))
        return info->name;
    return c_empty;
}

void GameObject::SetName(const std::string_view name) {
    if (!m_Registry) return;
    if (auto* info = m_Registry->try_get<CEntityInfo>(m_Entity))
        info->name = name;
}

// ── Scene ─────────────────────────────────────────────────────────────────────

Scene* GameObject::GetScene() const {
    if (!m_Registry) return nullptr;
    return m_Registry->ctx().get<Scene*>();
}

// ── Hierarchy ─────────────────────────────────────────────────────────────────

GameObject GameObject::GetParent() const {
    if (!m_Registry) return {};
    const auto* h = m_Registry->try_get<CHierarchy>(m_Entity);
    if (!h || h->parent == entt::null) return {};
    return GameObject(h->parent, m_Registry);
}

std::vector<GameObject> GameObject::GetChildren() const {
    if (!m_Registry) return {};
    const auto* h = m_Registry->try_get<CHierarchy>(m_Entity);
    if (!h) return {};
    std::vector<GameObject> result;
    result.reserve(h->children.size());
    for (auto child : h->children)
        result.emplace_back(child, m_Registry);
    return result;
}

void GameObject::SetParent(GameObject parent) {
    if (!m_Registry) return;
    auto* myH = m_Registry->try_get<CHierarchy>(m_Entity);
    if (!myH) return;

    if (myH->parent != entt::null) {
        auto& oldParentH = m_Registry->get<CHierarchy>(myH->parent);
        oldParentH.children.erase(
            std::remove(oldParentH.children.begin(), oldParentH.children.end(), m_Entity),
            oldParentH.children.end());
    }

    if (parent.IsValid()) {
        myH->parent = parent.GetEntity();
        m_Registry->get<CHierarchy>(parent.GetEntity()).children.push_back(m_Entity);
    } else {
        myH->parent = entt::null;
    }
}

void GameObject::AddChild(GameObject child) {
    if (child.IsValid() && child != *this)
        child.SetParent(*this);
}

void GameObject::RemoveChild(GameObject child) {
    if (!child.IsValid() || !m_Registry) return;
    auto* childH = m_Registry->try_get<CHierarchy>(child.GetEntity());
    if (childH && childH->parent == m_Entity) {
        auto* myH = m_Registry->try_get<CHierarchy>(m_Entity);
        if (myH) {
            myH->children.erase(
                std::remove(myH->children.begin(), myH->children.end(), child.GetEntity()),
                myH->children.end());
        }
        childH->parent = entt::null;
    }
}

void GameObject::ClearChildren() {
    if (!m_Registry) return;
    auto* myH = m_Registry->try_get<CHierarchy>(m_Entity);
    if (!myH) return;
    auto childrenCopy = myH->children;
    for (auto child : childrenCopy)
        RemoveChild(GameObject(child, m_Registry));
}

GameObject GameObject::FindChildByName(const std::string& name, const bool recursive) const {
    if (!m_Registry) return {};
    const auto* h = m_Registry->try_get<CHierarchy>(m_Entity);
    if (!h) return {};
    for (auto child : h->children) {
        GameObject go(child, m_Registry);
        if (go.GetName() == name) return go;
        if (recursive) {
            if (auto found = go.FindChildByName(name, true); found.IsValid())
                return found;
        }
    }
    return {};
}

// ── All Components (known-type list) ─────────────────────────────────────────

NOUS_Vector<Component*> GameObject::GetAllComponents() const {
    NOUS_Vector<Component*> result(MemoryTag::COMPONENT);
    if (!m_Registry) return result;
    if (auto* c = m_Registry->try_get<CTransform>(m_Entity)) result.push_back(c);
    if (auto* c = m_Registry->try_get<CMesh>     (m_Entity)) result.push_back(c);
    if (auto* c = m_Registry->try_get<CMaterial> (m_Entity)) result.push_back(c);
    if (auto* c = m_Registry->try_get<CCamera>   (m_Entity)) result.push_back(c);
    if (auto* c = m_Registry->try_get<CLight>    (m_Entity)) result.push_back(c);
    if (auto* c = m_Registry->try_get<CScript>   (m_Entity)) result.push_back(c);
    if (auto* c = m_Registry->try_get<CPrefab>   (m_Entity)) result.push_back(c);
    return result;
}

// ── Parent ID (deserialization helper) ────────────────────────────────────────

uint32_t GameObject::GetParentID() const {
    if (!m_Registry) return 0;
    const auto* h = m_Registry->try_get<CHierarchy>(m_Entity);
    if (!h || h->parent == entt::null) return 0;
    const auto* info = m_Registry->try_get<CEntityInfo>(h->parent);
    return info ? info->id : 0;
}

// ── Serialization ─────────────────────────────────────────────────────────────

JSON_Value* GameObject::Serialize() const {
    JSON_Value*  objVal = json_value_init_object();
    JSON_Object* obj    = json_value_get_object(objVal);

    const uint32_t myID     = GetID();
    const uint32_t parentID = GetParentID();

    json_object_set_number(obj, "uid",    myID);
    json_object_set_string(obj, "name",   GetName().c_str());
    json_object_set_number(obj, "parent", parentID);

    NOUS_INFO("Serializing: %s (ID: %u) -> Parent ID: %u", GetName().c_str(), myID, parentID);

    JSON_Value*  componentsVal = json_value_init_array();
    JSON_Array*  componentsArr = json_value_get_array(componentsVal);

    for (Component* c : GetAllComponents()) {
        if (JSON_Value* v = c->Serialize())
            json_array_append_value(componentsArr, v);
    }

    json_object_set_value(obj, "components", componentsVal);
    return objVal;
}

// static
GameObject GameObject::Deserialize(const JSON_Object* obj, Scene* scene) {
    const auto  uid      = static_cast<uint32_t>(json_object_get_number(obj, "uid"));
    const char* name     = json_object_get_string(obj, "name");
    const auto  parentID = static_cast<uint32_t>(json_object_get_number(obj, "parent"));

    entt::registry& reg    = const_cast<entt::registry&>(scene->GetRegistry());
    const entt::entity entity = reg.create();
    reg.emplace<CEntityInfo>(entity, uid, name ? name : "");
    reg.emplace<CHierarchy>(entity);

    GameObject go(entity, &reg);

    NOUS_INFO("Deserializing: %s (ID: %u) -> Parent ID: %u", name ? name : "", uid, parentID);

    if (const JSON_Array* comps = json_object_get_array(obj, "components")) {
        const size_t count = json_array_get_count(comps);
        for (size_t i = 0; i < count; ++i) {
            JSON_Object* compObj = json_array_get_object(comps, i);
            const char*  type    = json_object_get_string(compObj, "type");
            if (!type) continue;

            if      (strcmp(type, "CTransform") == 0) { go.AddComponent<CTransform>().Deserialize(compObj); }
            else if (strcmp(type, "CMesh")      == 0) { go.AddComponent<CMesh>()     .Deserialize(compObj); }
            else if (strcmp(type, "CMaterial")  == 0) { go.AddComponent<CMaterial>() .Deserialize(compObj); }
            else if (strcmp(type, "CCamera")    == 0) { go.AddComponent<CCamera>()   .Deserialize(compObj); }
            else if (strcmp(type, "CLight")     == 0) { go.AddComponent<CLight>()    .Deserialize(compObj); }
            else if (strcmp(type, "CScript")    == 0) { go.AddComponent<CScript>()   .Deserialize(compObj); }
            else if (strcmp(type, "CPrefab")    == 0) { go.AddComponent<CPrefab>()   .Deserialize(compObj); }
            else { NOUS_WARN("Unknown component type during deserialization: %s", type); }
        }
    }

    return go;
}
