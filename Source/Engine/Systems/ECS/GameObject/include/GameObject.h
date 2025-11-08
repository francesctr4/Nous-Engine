#ifndef NOUS_ENGINE_GAMEOBJECT_H
#define NOUS_ENGINE_GAMEOBJECT_H

#include "Engine/Systems/ECS/Component/Component.h"
#include "Engine/EngineExport.h"

#include <vector>
#include <unordered_map>
#include <typeindex>
#include <stdexcept>

typedef struct json_object_t JSON_Object;
typedef struct json_array_t  JSON_Array;
typedef struct json_value_t  JSON_Value;

// Forward declaration
class Scene;
class Component;

class GameObject {
public:
    GameObject();
    GameObject(uint32_t id, const std::string& name = "GameObject");
    ~GameObject();

    // ---------- Components ----------
    template<typename T, typename... Args>
    T& AddComponent(Args&&... args);

    void AddComponent(std::unique_ptr<Component> component);

    template<typename T> bool HasComponent() const;
    template<typename T> T& GetComponent();
    template<typename T> T* TryGetComponent();
    template<typename T> void RemoveComponent();

    void UpdateComponents(float deltaTime);
    std::vector<Component*> GetAllComponents();

    // ---------- Basic Info ----------
    NOUS_ENGINE_API uint32_t GetID() const;
    NOUS_ENGINE_API void SetName(const std::string& name);
    NOUS_ENGINE_API const std::string& GetName() const;

    // ---------- Hierarchy ----------
    NOUS_ENGINE_API GameObject* GetParent() const;
    NOUS_ENGINE_API const std::vector<GameObject*>& GetChildren() const;
    NOUS_ENGINE_API void SetParent(GameObject* parent);

    void AddChild(GameObject* child);
    void RemoveChild(GameObject* child);
    void ClearChildren();
    GameObject* FindChildByName(const std::string& name, bool recursive = true);

    // ---------- Serialization ----------
    JSON_Value* Serialize() const;
    static std::unique_ptr<GameObject> Deserialize(JSON_Object* obj);

    // For deserialization parent resolution
    uint32_t GetParentID() const;

private:
    uint32_t m_ID = 0;
    std::string m_Name;
    GameObject* m_Parent = nullptr;
    std::vector<GameObject*> m_Children;
    std::unordered_map<std::type_index, std::unique_ptr<Component>> m_Components;
    uint32_t m_ParentID = 0;
};

// -----------------------------------------------------------------------------
// Template Implementations
// -----------------------------------------------------------------------------
template<typename T, typename... Args>
T& GameObject::AddComponent(Args&&... args) {
    static_assert(std::is_base_of<Component, T>::value, "T must inherit from Component");
    auto component = std::make_unique<T>(std::forward<Args>(args)...);
    component->m_GameObject = this;
    T* ptr = component.get();
    m_Components[typeid(T)] = std::move(component);
    ptr->OnStart();
    return *ptr;
}

template<typename T>
bool GameObject::HasComponent() const {
    static_assert(std::is_base_of<Component, T>::value, "T must inherit from Component");
    return m_Components.find(typeid(T)) != m_Components.end();
}

template<typename T>
T& GameObject::GetComponent() {
    static_assert(std::is_base_of<Component, T>::value, "T must inherit from Component");
    auto it = m_Components.find(typeid(T));
    if (it == m_Components.end()) {
        throw std::runtime_error("Component not found: " + std::string(typeid(T).name()));
    }
    return *static_cast<T*>(it->second.get());
}

template<typename T>
T* GameObject::TryGetComponent() {
    static_assert(std::is_base_of<Component, T>::value, "T must inherit from Component");
    auto it = m_Components.find(typeid(T));
    if (it == m_Components.end()) {
        return nullptr;
    }
    return static_cast<T*>(it->second.get());
}

template<typename T>
void GameObject::RemoveComponent() {
    static_assert(std::is_base_of<Component, T>::value, "T must inherit from Component");
    auto it = m_Components.find(typeid(T));
    if (it != m_Components.end()) {
        it->second->OnDestroy();
        m_Components.erase(it);
    }
}

#endif // NOUS_ENGINE_GAMEOBJECT_H
