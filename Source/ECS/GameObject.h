#ifndef NOUS_ENGINE_GAMEOBJECT_H
#define NOUS_ENGINE_GAMEOBJECT_H

#include <entt/entt.hpp>
#include <string>

class GameObject {
public:
    GameObject() = default;
    GameObject(entt::entity handle, entt::registry* registry, const std::string& name = "GameObject")
            : m_Handle(handle), m_Registry(registry), m_Name(name) {}

    // ---------- Components ----------
    template<typename T, typename... Args>
    T& AddComponent(Args&&... args) {
        return m_Registry->emplace<T>(m_Handle, std::forward<Args>(args)...);
    }

    template<typename T>
    bool HasComponent() const {
        return m_Registry->all_of<T>(m_Handle);
    }

    template<typename T>
    T& GetComponent() {
        return m_Registry->get<T>(m_Handle);
    }

    template<typename T>
    void RemoveComponent() {
        m_Registry->remove<T>(m_Handle);
    }

    entt::entity GetHandle() const { return m_Handle; }

    // ---------- Name ----------
    void SetName(const std::string& name) { m_Name = name; }
    const std::string& GetName() const { return m_Name; }

    // ---------- Hierarchy ----------
    GameObject* GetParent() const { return m_Parent; }
    std::vector<GameObject*>& GetChildren() { return m_Children; }

    void SetParent(GameObject* parent) {
        if (m_Parent) {
            // remove from old parent's children
            auto& siblings = m_Parent->m_Children;
            siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());
        }

        m_Parent = parent;

        if (m_Parent) {
            m_Parent->m_Children.push_back(this);
        }
    }

    void AddChild(GameObject* child) {
        if (child) {
            child->SetParent(this);
        }
    }

private:
    entt::entity m_Handle{ entt::null };
    entt::registry* m_Registry = nullptr;

    std::string m_Name;
    GameObject* m_Parent = nullptr;
    std::vector<GameObject*> m_Children;
};

#endif //NOUS_ENGINE_GAMEOBJECT_H
