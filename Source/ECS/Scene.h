#ifndef NOUS_ENGINE_SCENE_H
#define NOUS_ENGINE_SCENE_H

#include "GameObject.h"
#include "ECS/Components/ComponentTransform.h"
#include "Core/Application.h"
#include "Modules/ModuleScene.h"
#include "Utils/Random.h"
#include <unordered_set>

class Scene {
public:
    Scene(const std::string& name = "Untitled Scene")
            : m_Name(name) {}

    // Create new GameObject, optionally parented
    GameObject* CreateGameObject(const std::string& name = "GameObject", GameObject* parent = nullptr) {
        std::lock_guard<std::mutex> lock(m_Mutex); // Acquire lock
        auto id = static_cast<uint32_t>(Random::Generate());
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

    // Helper to get GameObject by ID
    GameObject* GetGameObjectByID(uint32_t id) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        auto it = std::find_if(m_GameObjects.begin(), m_GameObjects.end(),
                               [id](const std::unique_ptr<GameObject>& go) {
                                   return go->GetID() == id;
                               });
        return (it != m_GameObjects.end()) ? it->get() : nullptr;
    }

// Create GameObject and return ID (not pointer)
    uint32_t CreateGameObjectID(const std::string& name = "GameObject", GameObject* parent = nullptr) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        auto id = static_cast<uint32_t>(Random::Generate());
        auto go = std::make_unique<GameObject>(id, this, name);
        GameObject* ptr = go.get();

        // Add default Transform component
        ptr->AddComponent<CTransform>();

        if (parent) {
            parent->AddChild(ptr);
        }

        m_GameObjects.push_back(std::move(go));
        return id; // Return the ID instead of pointer
    }

// Destroy GameObject by ID
    void DestroyGameObjectByID(uint32_t id) {
        GameObject* go = GetGameObjectByID(id);
        if (go) {
            DestroyGameObject(go); // Use your existing destroy method
        }
    }

    // Getters
    const std::string& GetName() const { return m_Name; }
    void SetName(const std::string& name) { m_Name = name; }

    std::vector<std::unique_ptr<GameObject>>& GetGameObjects() { return m_GameObjects; }

private:
    std::string m_Name;
    std::vector<std::unique_ptr<GameObject>> m_GameObjects;
    std::mutex m_Mutex; // protects m_GameObjects and m_NextID
};

#endif //NOUS_ENGINE_SCENE_H
