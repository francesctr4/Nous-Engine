#ifndef NOUS_ENGINE_SCENE_H
#define NOUS_ENGINE_SCENE_H

#include <Engine/ECS/GameObject.h>
#include <Engine/ECS/Components/ComponentTransform.h>
#include <Engine/Core/Application.h>
#include <Engine/Core/Modules/ModuleScene.h>
#include <Engine/Utils/Random.h>
#include <unordered_set>
#include <mutex>
#include <queue>
#include "Engine/Systems/Logging System/Logger.h"

class Scene {
public:
    Scene(const std::string& name = "Untitled Scene")
            : m_Name(name) {}

    ~Scene() {
        Clear();
    }

    // Create new GameObject, optionally parented
    GameObject* CreateGameObject(const std::string& name = "GameObject", GameObject* parent = nullptr) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        auto id = GenerateUniqueID();
        auto go = std::make_unique<GameObject>(id, name);
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

        // Collect all objects to destroy (the entire subtree)
        std::vector<GameObject*> toDestroy;
        CollectGameObjectTree(go, toDestroy);

        // Now destroy them all with a single lock
        std::lock_guard<std::mutex> lock(m_Mutex);

        // Destroy in reverse order (children first, then parents) to avoid dangling references
        for (auto it = toDestroy.rbegin(); it != toDestroy.rend(); ++it) {
            DestroySingleGameObject(*it);
        }
    }

    // Update all GameObjects in the scene
    void Update(float deltaTime) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        for (auto& gameObject : m_GameObjects) {
            gameObject->UpdateComponents(deltaTime);
        }
    }

    // Find GameObject by ID
    GameObject* FindGameObjectByID(uint32_t id) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return FindGameObjectByID_NoLock(id);
    }

    // Find GameObjects by name
    std::vector<GameObject*> FindGameObjectsByName(const std::string& name) {
        std::lock_guard<std::mutex> lock(m_Mutex);
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
        return FindGameObjectByID(id);
    }

    // Create GameObject and return ID
    uint32_t CreateGameObjectID(const std::string& name = "GameObject", GameObject* parent = nullptr) {
        auto* go = CreateGameObject(name, parent);
        return go ? go->GetID() : 0;
    }

    // Destroy GameObject by ID
    void DestroyGameObjectByID(uint32_t id) {
        GameObject* go = GetGameObjectByID(id);
        if (go) {
            DestroyGameObject(go);
        }
    }

    // Getters
    const std::string& GetName() const { return m_Name; }
    void SetName(const std::string& name) { m_Name = name; }

    // Note: This gives direct access to game objects - use with caution!
    std::vector<std::unique_ptr<GameObject>>& GetGameObjects() { return m_GameObjects; }

    void Serialize(const std::string& filepath) const {
        std::lock_guard<std::mutex> lock(m_Mutex);

        JSON_Value* root = json_value_init_object();
        JSON_Object* rootObj = json_value_get_object(root);

        JSON_Value* arrVal = json_value_init_array();
        JSON_Array* arr = json_value_get_array(arrVal);

        // Save ALL GameObjects regardless of their parent status
        for (const auto& obj : m_GameObjects) {
            json_array_append_value(arr, obj->Serialize());
        }

        // Add scene metadata
        json_object_set_string(rootObj, "name", m_Name.c_str());
        json_object_set_number(rootObj, "version", 0.1);

        json_object_set_value(rootObj, "GameObjects", arrVal);

        json_serialize_to_file_pretty(root, filepath.c_str());
        json_value_free(root);

        NOUS_INFO("Scene saved: %s with %zu objects", filepath.c_str(), m_GameObjects.size());
    }

    void Deserialize(const std::string& filepath) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        Clear();

        JSON_Value* root = json_parse_file(filepath.c_str());
        if (!root) {
            NOUS_ERROR("Failed to parse scene file: %s", filepath.c_str());
            return;
        }

        JSON_Object* rootObj = json_value_get_object(root);

        // Load scene metadata
        const char* sceneName = json_object_get_string(rootObj, "name");
        if (sceneName) {
            m_Name = sceneName;
        }

        JSON_Array* arr = json_object_get_array(rootObj, "GameObjects");
        if (!arr) {
            json_value_free(root);
            NOUS_WARN("No GameObjects array found in scene file");
            return;
        }

        size_t count = json_array_get_count(arr);

        // Pass 1: Create all GameObjects and store parent IDs
        std::vector<std::pair<std::unique_ptr<GameObject>, uint32_t>> gameObjectsWithParents;

        for (size_t i = 0; i < count; ++i) {
            JSON_Object* obj = json_array_get_object(arr, i);
            auto gameObject = GameObject::Deserialize(obj);
            if (gameObject) {
                uint32_t parentID = gameObject->GetParentID();
                gameObjectsWithParents.emplace_back(std::move(gameObject), parentID);
            }
        }

        // Pass 2a: Add ALL GameObjects to the scene first
        for (auto& [gameObject, parentID] : gameObjectsWithParents) {
            m_GameObjects.push_back(std::move(gameObject));
        }

        // Pass 2b: NOW establish parent-child relationships
        for (auto& gameObject : m_GameObjects) {
            uint32_t parentID = gameObject->GetParentID();
            if (parentID != 0) {
                GameObject* parent = FindGameObjectByID_NoLock(parentID);
                if (parent) {
                    parent->AddChild(gameObject.get());
                    NOUS_INFO("Set parent: %s (ID: %u) -> %s (ID: %u)",
                           gameObject->GetName().c_str(), gameObject->GetID(),
                           parent->GetName().c_str(), parentID);
                } else {
                    NOUS_WARN("Warning: Parent with ID %u not found for GameObject %s. It will be a root object.",
                           parentID, gameObject->GetName().c_str());
                }
            }
        }

        json_value_free(root);
        NOUS_WARN("[%s] Successfully loaded scene: %s with %zu objects", __FUNCTION__, filepath.c_str(), m_GameObjects.size());
    }

    void Clear() {
        // Don't clear External->scene->selectedGameObject here
        // Let the ModuleScene handle selection clearing when it destroys the scene
        // Clear parent-child relationships first to avoid issues
        for (auto& gameObject : m_GameObjects) {
            // Clear children safely
            auto children = gameObject->GetChildren(); // Get copy
            for (auto* child : children) {
                gameObject->RemoveChild(child);
            }
            gameObject->SetParent(nullptr);
        }

        // Now clear all objects
        m_GameObjects.clear();
    }

private:
    // Helper method to collect entire GameObject tree without locking
    void CollectGameObjectTree(GameObject* root, std::vector<GameObject*>& collection) {
        if (!root) return;

        // Use BFS to collect all objects in the subtree
        std::queue<GameObject*> queue;
        queue.push(root);

        while (!queue.empty()) {
            GameObject* current = queue.front();
            queue.pop();

            // Only add if not already in collection (avoid cycles)
            if (std::find(collection.begin(), collection.end(), current) == collection.end()) {
                collection.push_back(current);

                // Add children to queue (use copy to avoid modification during iteration)
                auto children = current->GetChildren();
                for (GameObject* child : children) {
                    queue.push(child);
                }
            }
        }
    }

    // Internal method to destroy a single GameObject (assumes lock is already held)
    void DestroySingleGameObject(GameObject* go) {
        if (!go) return;

        // If this object was selected → clear selection
        if (External->scene->selectedGameObject == go) {
            External->scene->selectedGameObject = nullptr;
        }

        // Remove from parent's children
        if (auto* parent = go->GetParent()) {
            parent->RemoveChild(go);
        }

        // Clear children safely
        auto children = go->GetChildren(); // Get copy
        for (auto* child : children) {
            go->RemoveChild(child);
        }

        // Find and remove from storage
        auto it = std::find_if(m_GameObjects.begin(), m_GameObjects.end(),
                               [go](const std::unique_ptr<GameObject>& obj) {
                                   return obj.get() == go;
                               });
        if (it != m_GameObjects.end()) {
            m_GameObjects.erase(it);
        }
    }

    // Internal find without locking (for use when lock is already held)
    GameObject* FindGameObjectByID_NoLock(uint32_t id) {
        auto it = std::find_if(m_GameObjects.begin(), m_GameObjects.end(),
                               [id](const std::unique_ptr<GameObject>& obj) {
                                   return obj->GetID() == id;
                               });
        return it != m_GameObjects.end() ? it->get() : nullptr;
    }

    uint32_t GenerateUniqueID() {
        uint32_t id;
        do {
            id = static_cast<uint32_t>(Random::Generate());
        } while (FindGameObjectByID_NoLock(id) != nullptr); // Ensure uniqueness
        return id;
    }

private:
    std::string m_Name;
    std::vector<std::unique_ptr<GameObject>> m_GameObjects;
    mutable std::mutex m_Mutex;
};

#endif //NOUS_ENGINE_SCENE_H