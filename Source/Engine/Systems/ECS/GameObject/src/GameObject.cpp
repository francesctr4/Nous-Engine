#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/ECS/Scene/include/Scene.h"
#include "Engine/Systems/ECS/ECSInternalComponents.h"

#include "Engine/Systems/ECS/Component/Types/ComponentTypes.h"
#include "Engine/Core/Logger/Logger.h"

#include "Engine/Utils/Serialization/JsonFile/JsonArray.h"
#include <ranges>
#include <algorithm>

// ── Identity ──────────────────────────────────────────────────────────────────

uint32_t GameObject::GetID() const {
    if (!m_registry) return 0;
    if (const auto* info = m_registry->try_get<CEntityInfo>(m_entity))
        return info->id;
    return 0;
}

const std::string& GameObject::GetName() const {
    static const std::string c_empty;
    if (!m_registry) return c_empty;
    if (const auto* info = m_registry->try_get<CEntityInfo>(m_entity))
        return info->name;
    return c_empty;
}

void GameObject::SetName(const std::string_view name) {
    if (!m_registry) return;
    if (auto* info = m_registry->try_get<CEntityInfo>(m_entity))
        info->name = name;
}

// ── Scene ─────────────────────────────────────────────────────────────────────

Scene* GameObject::GetScene() const {
    if (!m_registry) return nullptr;
    return m_registry->ctx().get<Scene*>();
}

// ── Hierarchy ─────────────────────────────────────────────────────────────────

GameObject GameObject::GetParent() const {
    if (!m_registry) return {};
    const auto* h = m_registry->try_get<CHierarchy>(m_entity);
    if (!h || h->parent == entt::null) return {};
    return GameObject(h->parent, m_registry);
}

std::vector<GameObject> GameObject::GetChildren() const {
    if (!m_registry) return {};
    const auto* h = m_registry->try_get<CHierarchy>(m_entity);
    if (!h) return {};
    std::vector<GameObject> result;
    result.reserve(h->children.size());
    for (auto child : h->children)
        result.emplace_back(child, m_registry);
    return result;
}

void GameObject::SetParent(GameObject parent) {
    if (!m_registry) return;
    auto* myH = m_registry->try_get<CHierarchy>(m_entity);
    if (!myH) return;

    if (myH->parent != entt::null) {
        auto& oldParentH = m_registry->get<CHierarchy>(myH->parent);
        oldParentH.children.erase(
            std::remove(oldParentH.children.begin(), oldParentH.children.end(), m_entity),
            oldParentH.children.end());
    }

    if (parent.IsValid()) {
        myH->parent = parent.GetEntity();
        m_registry->get<CHierarchy>(parent.GetEntity()).children.push_back(m_entity);
    } else {
        myH->parent = entt::null;
    }
}

void GameObject::AddChild(GameObject child) {
    if (child.IsValid() && child != *this)
        child.SetParent(*this);
}

void GameObject::RemoveChild(GameObject child) {
    if (!child.IsValid() || !m_registry) return;
    auto* childH = m_registry->try_get<CHierarchy>(child.GetEntity());
    if (childH && childH->parent == m_entity) {
        auto* myH = m_registry->try_get<CHierarchy>(m_entity);
        if (myH) {
            myH->children.erase(
                std::remove(myH->children.begin(), myH->children.end(), child.GetEntity()),
                myH->children.end());
        }
        childH->parent = entt::null;
    }
}

void GameObject::ClearChildren() {
    if (!m_registry) return;
    auto* myH = m_registry->try_get<CHierarchy>(m_entity);
    if (!myH) return;
    auto childrenCopy = myH->children;
    for (auto child : childrenCopy)
        RemoveChild(GameObject(child, m_registry));
}

GameObject GameObject::FindChildByName(const std::string& name, const bool recursive) const {
    if (!m_registry) return {};
    const auto* h = m_registry->try_get<CHierarchy>(m_entity);
    if (!h) return {};
    for (auto child : h->children) {
        GameObject go(child, m_registry);
        if (go.GetName() == name) return go;
        if (recursive) {
            if (auto found = go.FindChildByName(name, true); found.IsValid())
                return found;
        }
    }
    return {};
}

// ── All Components (known-type list) ─────────────────────────────────────────

std::vector<Component*> GameObject::GetAllComponents() const {
    std::vector<Component*> result;
    if (!m_registry) return result;
    ComponentTypes::ForEachPresent(*m_registry, m_entity,
        [&](Component* c) { result.push_back(c); });
    return result;
}

// ── Parent ID (deserialization helper) ────────────────────────────────────────

uint32_t GameObject::GetParentID() const {
    if (!m_registry) return 0;
    const auto* h = m_registry->try_get<CHierarchy>(m_entity);
    if (!h || h->parent == entt::null) return 0;
    const auto* info = m_registry->try_get<CEntityInfo>(h->parent);
    return info ? info->id : 0;
}

// ── Serialization ─────────────────────────────────────────────────────────────

JsonObject GameObject::Serialize() const {
    const uint32_t myID     = GetID();
    const uint32_t parentID = GetParentID();

    NOUS_INFO("Serializing: %s (ID: %u) -> Parent ID: %u", GetName().c_str(), myID, parentID);

    JsonArray componentsArr;
    for (Component* c : GetAllComponents())
        componentsArr.Append(c->Serialize());

    JsonObject obj;
    obj.Set("uid",        static_cast<double>(myID));
    obj.Set("name",       GetName());
    obj.Set("parent",     static_cast<double>(parentID));
    obj.Set("components", std::move(componentsArr));
    return obj;
}

// static
GameObject GameObject::Deserialize(const JsonObject& obj, Scene* scene) {
    const auto        uid      = static_cast<uint32_t>(obj.GetDouble("uid",    0.0));
    const std::string name     = obj.GetString("name");
    const auto        parentID = static_cast<uint32_t>(obj.GetDouble("parent", 0.0));

    entt::registry& reg = scene->GetRegistry();
    const entt::entity entity = reg.create();
    reg.emplace<CEntityInfo>(entity, uid, name);
    reg.emplace<CHierarchy>(entity);

    GameObject go(entity, &reg);

    NOUS_INFO("Deserializing: %s (ID: %u) -> Parent ID: %u", name.c_str(), uid, parentID);

    JsonArray comps = obj.GetArray("components");
    if (!comps.IsEmpty()) {
        const int count = comps.Count();
        for (int i = 0; i < count; ++i) {
            JsonObject compObj = comps.GetObject(i);
            const std::string type = compObj.GetString("type");
            if (type.empty()) continue;

            if (Component* c = ComponentTypes::AddByName(go, type))
                c->Deserialize(compObj);
            else
                NOUS_WARN("Unknown component type during deserialization: %s", type.c_str());
        }
    }

    return go;
}
