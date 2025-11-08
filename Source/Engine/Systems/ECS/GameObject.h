#ifndef NOUS_ENGINE_GAMEOBJECT_H
#define NOUS_ENGINE_GAMEOBJECT_H

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <typeindex>
#include <algorithm>

#include "Component.h"
#include "Engine/Systems/ECS/Components/ComponentTransform.h"
// Parson
#include "parson.h"
#include "Engine/Core/Logging System/Logger.h"

// Forward declarations
class Scene;

class GameObject {
public:
    GameObject() = default;

    GameObject(uint32_t id, const std::string& name = "GameObject")
            : m_ID(id), m_Name(name) {}

    ~GameObject() {
        // Call OnDestroy for all components
        for (auto& [type, component] : m_Components) {
            component->OnDestroy();
        }

        // Clear components (invokes unique_ptr destructors)
        m_Components.clear();

        // Clear children to avoid dangling pointers
        ClearChildren();
    }

    // ---------- Components ----------
    template<typename T, typename... Args>
    T& AddComponent(Args&&... args) {
        static_assert(std::is_base_of<Component, T>::value, "T must inherit from Component");

        auto component = std::make_unique<T>(std::forward<Args>(args)...);
        component->m_GameObject = this;
        T* ptr = component.get();

        m_Components[typeid(T)] = std::move(component);
        ptr->OnStart(); // Call OnStart after adding
        return *ptr;
    }

    // Add component from unique_ptr (for deserialization)
    void AddComponent(std::unique_ptr<Component> component) {
        if (!component) return;

        component->m_GameObject = this;
        auto type = std::type_index(typeid(*component));
        Component* ptr = component.get();

        m_Components[type] = std::move(component);
        ptr->OnStart();
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

    // ---------- Name ----------
    void SetName(const std::string& name) { m_Name = name; }
    const std::string& GetName() const { return m_Name; }

    // ---------- Hierarchy ----------
    GameObject* GetParent() const { return m_Parent; }
    const std::vector<GameObject*>& GetChildren() const { return m_Children; } // Return const reference

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

    // Clear all children safely
    void ClearChildren() {
        // Make a copy since RemoveChild modifies m_Children
        auto childrenCopy = m_Children;
        for (auto* child : childrenCopy) {
            RemoveChild(child);
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

    JSON_Value* Serialize() const {
        JSON_Value* objVal = json_value_init_object();
        JSON_Object* obj = json_value_get_object(objVal);

        json_object_set_number(obj, "uid", m_ID);
        json_object_set_string(obj, "name", m_Name.c_str());

        // Store parent ID (0 if no parent) - THIS IS CRITICAL
        uint32_t parentID = m_Parent ? m_Parent->GetID() : 0;
        json_object_set_number(obj, "parent", parentID);

        NOUS_INFO("Serializing: %s (ID: %u) -> Parent ID: %u",
               m_Name.c_str(), m_ID, parentID);

        // Serialize components
        JSON_Value* componentsVal = json_value_init_array();
        JSON_Array* componentsArr = json_value_get_array(componentsVal);

        for (const auto& [type, component] : m_Components) {
            json_array_append_value(componentsArr, component->Serialize());
        }

        json_object_set_value(obj, "components", componentsVal);
        return objVal;
    }

    static std::unique_ptr<GameObject> Deserialize(JSON_Object* obj) {
        uint32_t uid = static_cast<uint32_t>(json_object_get_number(obj, "uid"));
        const char* name = json_object_get_string(obj, "name");
        uint32_t parentID = static_cast<uint32_t>(json_object_get_number(obj, "parent"));

        auto go = std::make_unique<GameObject>(uid, name ? name : "");

        // Store the parent ID for later resolution
        go->m_ParentID = parentID;

        NOUS_INFO("Deserializing: %s (ID: %u) -> Parent ID: %u",
               name ? name : "", uid, parentID);

        // Deserialize components
        JSON_Array* comps = json_object_get_array(obj, "components");
        if (comps) {
            size_t count = json_array_get_count(comps);
            for (size_t i = 0; i < count; ++i) {
                JSON_Object* compObj = json_array_get_object(comps, i);
                const char* type = json_object_get_string(compObj, "type");

                if (type) {
                    auto component = Component::CreateComponent(type);
                    if (component) {
                        component->Deserialize(compObj);
                        go->AddComponent(std::move(component));
                    }
                }
            }
        }

        return go;
    }

    // Helper for scene deserialization
    uint32_t GetParentID() const { return m_ParentID; }

private:
    uint32_t m_ID = 0;
    std::string m_Name;
    GameObject* m_Parent = nullptr;
    std::vector<GameObject*> m_Children;
    std::unordered_map<std::type_index, std::unique_ptr<Component>> m_Components;

    // Temporary storage for deserialization
    uint32_t m_ParentID = 0;
};

#endif //NOUS_ENGINE_GAMEOBJECT_H
