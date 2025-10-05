#ifndef NOUS_ENGINE_GAMEOBJECT_H
#define NOUS_ENGINE_GAMEOBJECT_H

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <typeindex>
#include <algorithm>

#include "ECS/Component.h"

// Forward declarations
class Scene;

class GameObject {
public:
    GameObject() = default;
    GameObject(uint32_t id, Scene* scene, const std::string& name = "GameObject")
            : m_ID(id), m_Scene(scene), m_Name(name) {}

    // ---------- Components ----------
    template<typename T, typename... Args>
    T& AddComponent(Args&&... args) {
        static_assert(std::is_base_of<Component, T>::value, "T must inherit from Component");

        auto component = std::make_unique<T>(std::forward<Args>(args)...);
        component->m_GameObject = this;
        T* ptr = component.get();

        // Call OnStart when component is added
        component->OnStart();

        m_Components[typeid(T)] = std::move(component);
        return *ptr;
    }

    template<typename T>
    bool HasComponent() const {
        static_assert(std::is_base_of<Component, T>::value, "T must inherit from Component");
        return m_Components.find(typeid(T)) != m_Components.end();
    }

    template<typename T>
    T& GetComponent() {
        static_assert(std::is_base_of<Component, T>::value, "T must inherit from Component");
        auto it = m_Components.find(typeid(T));
        if (it == m_Components.end()) {
            throw std::runtime_error("Component not found: " + std::string(typeid(T).name()));
        }
        return *static_cast<T*>(it->second.get());
    }

    template<typename T>
    T* TryGetComponent() {
        static_assert(std::is_base_of<Component, T>::value, "T must inherit from Component");
        auto it = m_Components.find(typeid(T));
        if (it == m_Components.end()) {
            return nullptr;
        }
        return static_cast<T*>(it->second.get());
    }

    template<typename T>
    void RemoveComponent() {
        static_assert(std::is_base_of<Component, T>::value, "T must inherit from Component");
        auto it = m_Components.find(typeid(T));
        if (it != m_Components.end()) {
            it->second->OnDestroy();
            m_Components.erase(it);
        }
    }

    // Update all components on this GameObject
    void UpdateComponents(float deltaTime) {
        for (auto& [type, component] : m_Components) {
            component->OnUpdate(deltaTime);
        }
    }

    // Get all components (for iteration)
    std::vector<Component*> GetAllComponents() {
        std::vector<Component*> components;
        for (auto& [type, component] : m_Components) {
            components.push_back(component.get());
        }
        return components;
    }

    uint32_t GetID() const { return m_ID; }
    Scene* GetScene() const { return m_Scene; }

    // ---------- Name ----------
    void SetName(const std::string& name) { m_Name = name; }
    const std::string& GetName() const { return m_Name; }

    // ---------- Hierarchy ----------
    GameObject* GetParent() const { return m_Parent; }
    std::vector<GameObject*>& GetChildren() { return m_Children; }

    void SetParent(GameObject* parent) {
        if (m_Parent == parent) return;

        if (m_Parent) {
            // Remove from old parent's children
            auto& siblings = m_Parent->m_Children;
            siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());
        }

        m_Parent = parent;

        if (m_Parent) {
            m_Parent->m_Children.push_back(this);
        }
    }

    void AddChild(GameObject* child) {
        if (child && child != this) {
            child->SetParent(this);
        }
    }

    void RemoveChild(GameObject* child) {
        if (child) {
            auto it = std::find(m_Children.begin(), m_Children.end(), child);
            if (it != m_Children.end()) {
                m_Children.erase(it);
                child->m_Parent = nullptr;
            }
        }
    }

    // Recursively find child by name
    GameObject* FindChildByName(const std::string& name, bool recursive = true) {
        for (auto* child : m_Children) {
            if (child->GetName() == name) {
                return child;
            }
            if (recursive) {
                GameObject* found = child->FindChildByName(name, true);
                if (found) return found;
            }
        }
        return nullptr;
    }

private:
    uint32_t m_ID = 0;
    Scene* m_Scene = nullptr;
    std::string m_Name;
    GameObject* m_Parent = nullptr;
    std::vector<GameObject*> m_Children;
    std::unordered_map<std::type_index, std::unique_ptr<Component>> m_Components;
};

#endif //NOUS_ENGINE_GAMEOBJECT_H
