#ifndef NOUS_ENGINE_GAMEOBJECT_H
#define NOUS_ENGINE_GAMEOBJECT_H

#include "Engine/Systems/ECS/Component/Component.h"
#include "Engine/EngineExport.h"
#include "Engine/Utils/DataStructures/NOUS_Vector.h"

#include <vector>
#include <unordered_map>
#include <typeindex>
#include <stdexcept>

typedef struct json_object_t JSON_Object;
typedef struct json_array_t  JSON_Array;
typedef struct json_value_t  JSON_Value;

// Forward declarations
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

    void AddComponent(Component* component);

    template<typename T> bool HasComponent() const;
    template<typename T> T& GetComponent();
    template<typename T> T* TryGetComponent();
    template<typename T> void RemoveComponent();
    void RemoveComponent(std::type_index typeIndex);

    void UpdateComponents(float deltaTime);
    NOUS_Vector<Component*> GetAllComponents();

    // ---------- Scene ----------
    Scene* GetScene() const { return m_Scene; }

    // ---------- Basic Info ----------
    NOUS_ENGINE_API uint32_t GetID() const;
    NOUS_ENGINE_API void SetName(const std::string& name);
    NOUS_ENGINE_API const std::string& GetName() const;

    // ---------- Hierarchy ----------
    NOUS_ENGINE_API GameObject* GetParent() const;
    NOUS_ENGINE_API const NOUS_Vector<GameObject*>& GetChildren() const;
    NOUS_ENGINE_API void SetParent(GameObject* parent);

    void AddChild(GameObject* child);
    void RemoveChild(GameObject* child);
    void ClearChildren();
    GameObject* FindChildByName(const std::string& name, bool recursive = true);

    // ---------- Serialization ----------
    JSON_Value* Serialize() const;
    static GameObject* Deserialize(JSON_Object* obj, Scene* scene = nullptr);

    // For deserialization parent resolution
    uint32_t GetParentID() const;

private:
    friend class Scene;

    uint32_t m_ID = 0;
    std::string m_Name;
    GameObject* m_Parent = nullptr;
    NOUS_Vector<GameObject*> m_Children;
    std::unordered_map<std::type_index, Component*> m_Components;
    uint32_t m_ParentID = 0;
    Scene* m_Scene = nullptr;
};

// -----------------------------------------------------------------------------
// Template Implementations
// -----------------------------------------------------------------------------
template<typename T, typename... Args>
T& GameObject::AddComponent(Args&&... args) {
    static_assert(std::is_base_of<Component, T>::value, "T must inherit from Component");

    T* component = NOUS_NEW<T>(MemoryTag::COMPONENT, std::forward<Args>(args)...);
    component->m_GameObject = this;

    auto it = m_Components.find(typeid(T));
    if (it != m_Components.end()) {
        Component* old = it->second;
        if (old) {
            old->OnDestroy();
            NOUS_DELETE(old, MemoryTag::COMPONENT);
        }
        it->second = component;
    } else {
        m_Components[typeid(T)] = component;
    }

    component->OnStart();
    return *component;
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
    return *static_cast<T*>(it->second);
}

template<typename T>
T* GameObject::TryGetComponent() {
    static_assert(std::is_base_of<Component, T>::value, "T must inherit from Component");
    auto it = m_Components.find(typeid(T));
    if (it == m_Components.end()) {
        return nullptr;
    }
    return static_cast<T*>(it->second);
}

template<typename T>
void GameObject::RemoveComponent() {
    static_assert(std::is_base_of<Component, T>::value, "T must inherit from Component");
    auto it = m_Components.find(typeid(T));
    if (it != m_Components.end()) {
        Component* comp = it->second;
        if (comp) {
            comp->OnDestroy();
            NOUS_DELETE(comp, MemoryTag::COMPONENT);
        }
        m_Components.erase(it);
    }
}

#endif // NOUS_ENGINE_GAMEOBJECT_H
