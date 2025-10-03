#ifndef NOUS_ENGINE_SCENE_H
#define NOUS_ENGINE_SCENE_H

#include <entt/entt.hpp>
#include "GameObject.h"

class Scene {
public:

    Scene(const std::string& name = "Untitled Scene")
            : m_Name(name) {}

    // Create new GameObject, optionally parented
    GameObject* CreateGameObject(const std::string& name = "GameObject", GameObject* parent = nullptr) {
        auto entity = m_Registry.create();
        auto go = std::make_unique<GameObject>(entity, &m_Registry, name);
        GameObject* ptr = go.get();

        if (parent) {
            parent->AddChild(ptr);
        }

        m_GameObjects.push_back(std::move(go));
        return ptr;
    }

    void DestroyGameObject(GameObject* go) {
        if (!go) return;

        // Recursively destroy children
        auto childrenCopy = go->GetChildren(); // copy, since we'll modify
        for (auto* child : childrenCopy) {
            DestroyGameObject(child);
        }

        // Remove from parent’s children
        if (auto* parent = go->GetParent()) {
            auto& siblings = parent->GetChildren();
            siblings.erase(std::remove(siblings.begin(), siblings.end(), go), siblings.end());
        }

        // Destroy from registry
        if (go->GetHandle() != entt::null) {
            m_Registry.destroy(go->GetHandle());
        }

        // Remove from storage
        m_GameObjects.erase(
                std::remove_if(m_GameObjects.begin(), m_GameObjects.end(),
                               [&](const std::unique_ptr<GameObject>& obj) { return obj.get() == go; }),
                m_GameObjects.end()
        );
    }

    // Getters
    const std::string& GetName() const { return m_Name; }
    void SetName(const std::string& name) { m_Name = name; }

    std::vector<std::unique_ptr<GameObject>>& GetGameObjects() { return m_GameObjects; }
    entt::registry& GetRegistry() { return m_Registry; }

private:
    std::string m_Name;
    entt::registry m_Registry;
    std::vector<std::unique_ptr<GameObject>> m_GameObjects;
};

#endif //NOUS_ENGINE_SCENE_H
