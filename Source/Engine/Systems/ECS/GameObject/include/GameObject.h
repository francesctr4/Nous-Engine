#pragma once

#include "Engine/Systems/ECS/Component/Component.h"
#include "Engine/EngineExport.h"
#include "Engine/Utils/DataStructures/NOUS_Vector.h"

#include <entt/entt.hpp>
#include <string>
#include <vector>
#include <stdexcept>

using JSON_Object = json_object_t;
using JSON_Array  = json_array_t;
using JSON_Value  = json_value_t;

class Scene;

// Lightweight value-type handle to an entity in an entt::registry.
// Default-constructed handle is null (IsValid() == false).
// Copy and move are cheap (16 bytes total).
class GameObject {
public:
    GameObject() = default;
    GameObject(entt::entity entity, entt::registry* registry)
        : m_Entity(entity), m_Registry(registry) {}

    // Returns false if the entity has been destroyed or this is a null handle.
    bool IsValid() const {
        return m_Registry != nullptr && m_Registry->valid(m_Entity);
    }

    // Internal EnTT entity ID — do not serialize.
    entt::entity GetEntity() const { return m_Entity; }

    // ── Components ────────────────────────────────────────────────────────────

    template<typename T, typename... Args>
    T& AddComponent(Args&&... args);

    template<typename T> bool  HasComponent()    const;
    template<typename T> T&    GetComponent();
    template<typename T> T*    TryGetComponent();
    template<typename T> void  RemoveComponent();

    // Returns all user-facing components attached to this entity.
    NOUS_Vector<Component*> GetAllComponents() const;

    // ── Identity ──────────────────────────────────────────────────────────────

    NOUS_ENGINE_API uint32_t           GetID()   const;
    NOUS_ENGINE_API const std::string& GetName() const;
    NOUS_ENGINE_API void               SetName(std::string_view name);

    // ── Scene ─────────────────────────────────────────────────────────────────
    NOUS_ENGINE_API Scene* GetScene() const;

    // ── Hierarchy ─────────────────────────────────────────────────────────────

    NOUS_ENGINE_API GameObject              GetParent()   const;
    NOUS_ENGINE_API std::vector<GameObject> GetChildren() const;
    NOUS_ENGINE_API void                    SetParent(GameObject parent);

    void AddChild(GameObject child);
    void RemoveChild(GameObject child);
    void ClearChildren();
    GameObject FindChildByName(const std::string& name, bool recursive = true) const;

    // ── Serialization ─────────────────────────────────────────────────────────

    JSON_Value*       Serialize() const;
    static GameObject Deserialize(const JSON_Object* obj, Scene* scene);

    // Used during two-phase deserialization to wire parent relationships.
    uint32_t GetParentID() const;

    // ── Comparison ────────────────────────────────────────────────────────────

    bool operator==(const GameObject& other) const {
        return m_Entity == other.m_Entity && m_Registry == other.m_Registry;
    }
    bool operator!=(const GameObject& other) const { return !(*this == other); }

private:
    friend class Scene;

    entt::entity    m_Entity   = entt::null;
    entt::registry* m_Registry = nullptr;
};

// ── Template implementations ──────────────────────────────────────────────────

template<typename T, typename... Args>
T& GameObject::AddComponent(Args&&... args) {
    static_assert(std::is_base_of_v<Component, T>, "T must inherit from Component");
    T& comp = m_Registry->emplace_or_replace<T>(m_Entity, std::forward<Args>(args)...);
    comp.m_Entity   = m_Entity;
    comp.m_Registry = m_Registry;
    comp.OnStart();
    return comp;
}

template<typename T>
bool GameObject::HasComponent() const {
    static_assert(std::is_base_of_v<Component, T>, "T must inherit from Component");
    return m_Registry != nullptr && m_Registry->all_of<T>(m_Entity);
}

template<typename T>
T& GameObject::GetComponent() {
    static_assert(std::is_base_of_v<Component, T>, "T must inherit from Component");
    T* comp = m_Registry->try_get<T>(m_Entity);
    if (!comp)
        throw std::invalid_argument("Component not found: " + std::string(typeid(T).name()));
    return *comp;
}

template<typename T>
T* GameObject::TryGetComponent() {
    static_assert(std::is_base_of_v<Component, T>, "T must inherit from Component");
    if (!m_Registry) return nullptr;
    return m_Registry->try_get<T>(m_Entity);
}

template<typename T>
void GameObject::RemoveComponent() {
    static_assert(std::is_base_of_v<Component, T>, "T must inherit from Component");
    if (!m_Registry) return;
    if (T* comp = m_Registry->try_get<T>(m_Entity)) {
        comp->OnDestroy();
        m_Registry->erase<T>(m_Entity);
    }
}
