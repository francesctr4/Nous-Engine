#pragma once

#include <Logger/Asserts.h>
#include "Engine/Systems/ECS/Component/Component.h"
#include "Engine/EngineExport.h"

#include <entt/entt.hpp>
#include <string>
#include <vector>
#include <stdexcept>

class Scene;

// Lightweight value-type handle to an entity in an entt::registry.
// Default-constructed handle is null (IsValid() == false).
// Copy and move are cheap (16 bytes total).
class GameObject {
public:
    GameObject() = default;
    GameObject(entt::entity entity, entt::registry* registry)
        : m_entity(entity), m_registry(registry) {}

    // Returns false if the entity has been destroyed or this is a null handle.
    bool IsValid() const {
        return m_registry != nullptr && m_registry->valid(m_entity);
    }

    // Internal EnTT entity ID — do not serialize.
    entt::entity GetEntity() const { return m_entity; }

    // ── Components ────────────────────────────────────────────────────────────

    template<typename T, typename... Args>
    T& AddComponent(Args&&... args);

    template<typename T> bool  HasComponent()    const;
    template<typename T> T&    GetComponent();
    template<typename T> T*    TryGetComponent();
    template<typename T> void  RemoveComponent();

    // Returns all user-facing components attached to this entity.
    NOUS_ENGINE_API std::vector<Component*> GetAllComponents() const;

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

    JsonObject        Serialize() const;
    static GameObject Deserialize(const JsonObject& obj, Scene* scene);

    // Used during two-phase deserialization to wire parent relationships.
    uint32_t GetParentID() const;

    // ── Comparison ────────────────────────────────────────────────────────────

    bool operator==(const GameObject& other) const {
        return m_entity == other.m_entity && m_registry == other.m_registry;
    }
    bool operator!=(const GameObject& other) const { return !(*this == other); }

private:
    friend class Scene;

    entt::entity    m_entity   = entt::null;
    entt::registry* m_registry = nullptr;
};

// ── Template implementations ──────────────────────────────────────────────────

template<typename T, typename... Args>
T& GameObject::AddComponent(Args&&... args) {
    static_assert(std::is_base_of_v<Component, T>, "T must inherit from Component");
    NOUS_ASSERT_MAIN_THREAD();
    T& comp = m_registry->emplace_or_replace<T>(m_entity, std::forward<Args>(args)...);
    comp.m_entity   = m_entity;
    comp.m_registry = m_registry;
    comp.OnStart();
    return comp;
}

template<typename T>
bool GameObject::HasComponent() const {
    static_assert(std::is_base_of_v<Component, T>, "T must inherit from Component");
    return m_registry != nullptr && m_registry->all_of<T>(m_entity);
}

template<typename T>
T& GameObject::GetComponent() {
    static_assert(std::is_base_of_v<Component, T>, "T must inherit from Component");
    T* comp = m_registry->try_get<T>(m_entity);
    if (!comp)
        throw std::invalid_argument("Component not found: " + std::string(typeid(T).name()));
    return *comp;
}

template<typename T>
T* GameObject::TryGetComponent() {
    static_assert(std::is_base_of_v<Component, T>, "T must inherit from Component");
    if (!m_registry) return nullptr;
    return m_registry->try_get<T>(m_entity);
}

template<typename T>
void GameObject::RemoveComponent() {
    static_assert(std::is_base_of_v<Component, T>, "T must inherit from Component");
    if (!m_registry) return;
    if (T* comp = m_registry->try_get<T>(m_entity)) {
        comp->OnDestroy();
        m_registry->erase<T>(m_entity);
    }
}
