#ifndef NOUS_ENGINE_SCENE_H
#define NOUS_ENGINE_SCENE_H

#include "GameObject.h"
#include "ECS/Components/ComponentTransform.h"
#include "Core/Application.h"
#include "Modules/ModuleScene.h"
#include <unordered_set>

class Scene {
public:
    Scene(const std::string& name = "Untitled Scene")
            : m_Name(name), m_NextID(1) {}

    // Create new GameObject, optionally parented
    GameObject* CreateGameObject(const std::string& name = "GameObject", GameObject* parent = nullptr) {
        std::lock_guard<std::mutex> lock(m_Mutex); // Acquire lock
        uint32_t id = m_NextID++;
        auto go = std::make_unique<GameObject>(id, this, name);
        GameObject* ptr = go.get();

        // Add default Transform component
        ptr->AddComponent<CTransform>();

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

        // If this object was selected → clear selection
        if (External->scene->selectedGameObject == go) {
            External->scene->selectedGameObject = nullptr;
        }

        // Remove from parent's children
        if (auto* parent = go->GetParent()) {
            auto& siblings = parent->GetChildren();
            siblings.erase(std::remove(siblings.begin(), siblings.end(), go), siblings.end());
        }

        // Remove from storage
        m_GameObjects.erase(
                std::remove_if(m_GameObjects.begin(), m_GameObjects.end(),
                               [&](const std::unique_ptr<GameObject>& obj) { return obj.get() == go; }),
                m_GameObjects.end()
        );
    }

    // Update all components in the scene
    void Update(float deltaTime) {
        for (auto& gameObject : m_GameObjects) {
            // You could add component iteration here if needed
            // For now, components update through their GameObject if needed
        }
    }

    // Find GameObject by ID
    GameObject* FindGameObjectByID(uint32_t id) {
        auto it = std::find_if(m_GameObjects.begin(), m_GameObjects.end(),
                               [id](const std::unique_ptr<GameObject>& obj) {
                                   return obj->GetID() == id;
                               });
        return it != m_GameObjects.end() ? it->get() : nullptr;
    }

    // Find GameObjects by name
    std::vector<GameObject*> FindGameObjectsByName(const std::string& name) {
        std::vector<GameObject*> result;
        for (auto& gameObject : m_GameObjects) {
            if (gameObject->GetName() == name) {
                result.push_back(gameObject.get());
            }
        }
        return result;
    }

    // Getters
    const std::string& GetName() const { return m_Name; }
    void SetName(const std::string& name) { m_Name = name; }

    std::vector<std::unique_ptr<GameObject>>& GetGameObjects() { return m_GameObjects; }

private:
    std::string m_Name;
    std::vector<std::unique_ptr<GameObject>> m_GameObjects;
    uint32_t m_NextID;
    std::mutex m_Mutex; // protects m_GameObjects and m_NextID
};

#endif //NOUS_ENGINE_SCENE_H
