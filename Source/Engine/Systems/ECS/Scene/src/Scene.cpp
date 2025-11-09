#include "Engine/Systems/ECS/Scene/include/Scene.h"

#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/ECS/Component/CTransform/include/CTransform.h"
#include "Engine/Core/Application.h"
#include "Engine/Modules/ModuleScene/include/ModuleScene.h"
#include "Engine/Utils/Serialization/Random/Random.h"
#include "Engine/Core/Logging System/Logger.h"

#include <queue>
#include <parson.h>

// -----------------------------------------------------------------------------
// Constructor / Destructor
// -----------------------------------------------------------------------------
Scene::Scene(const std::string& name)
        : m_Name(name) {}

Scene::~Scene() {
    Clear();
}

// -----------------------------------------------------------------------------
// GameObject Creation & Destruction
// -----------------------------------------------------------------------------
GameObject* Scene::CreateGameObject(const std::string& name, GameObject* parent) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    auto id = GenerateUniqueID();
    auto go = std::make_unique<GameObject>(id, name);
    GameObject* ptr = go.get();

    ptr->AddComponent<CTransform>();

    if (parent) {
        parent->AddChild(ptr);
    }

    m_GameObjects.push_back(std::move(go));
    return ptr;
}

void Scene::DestroyGameObject(GameObject* go) {
    if (!go) return;

    std::vector<GameObject*> toDestroy;
    CollectGameObjectTree(go, toDestroy);

    std::lock_guard<std::mutex> lock(m_Mutex);

    for (auto it = toDestroy.rbegin(); it != toDestroy.rend(); ++it) {
        DestroySingleGameObject(*it);
    }
}

// -----------------------------------------------------------------------------
// Update
// -----------------------------------------------------------------------------
void Scene::Update(float deltaTime) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    for (auto& gameObject : m_GameObjects) {
        gameObject->UpdateComponents(deltaTime);
    }
}

// -----------------------------------------------------------------------------
// Lookup Functions
// -----------------------------------------------------------------------------
GameObject* Scene::FindGameObjectByID(uint32_t id) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    return FindGameObjectByID_NoLock(id);
}

std::vector<GameObject*> Scene::FindGameObjectsByName(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    std::vector<GameObject*> result;
    for (auto& gameObject : m_GameObjects) {
        if (gameObject->GetName() == name) {
            result.push_back(gameObject.get());
        }
    }
    return result;
}

GameObject* Scene::GetGameObjectByID(uint32_t id) {
    return FindGameObjectByID(id);
}

// -----------------------------------------------------------------------------
// GameObject ID Utilities
// -----------------------------------------------------------------------------
uint32_t Scene::CreateGameObjectID(const std::string& name, GameObject* parent) {
    auto* go = CreateGameObject(name, parent);
    return go ? go->GetID() : 0;
}

void Scene::DestroyGameObjectByID(uint32_t id) {
    GameObject* go = GetGameObjectByID(id);
    if (go) DestroyGameObject(go);
}

// -----------------------------------------------------------------------------
// Getters / Setters
// -----------------------------------------------------------------------------
const std::string& Scene::GetName() const { return m_Name; }
void Scene::SetName(const std::string& name) { m_Name = name; }

std::vector<std::unique_ptr<GameObject>>& Scene::GetGameObjects() { return m_GameObjects; }

// -----------------------------------------------------------------------------
// Serialization
// -----------------------------------------------------------------------------
void Scene::Serialize(const std::string& filepath) const {
    std::lock_guard<std::mutex> lock(m_Mutex);

    JSON_Value* root = json_value_init_object();
    JSON_Object* rootObj = json_value_get_object(root);
    JSON_Value* arrVal = json_value_init_array();
    JSON_Array* arr = json_value_get_array(arrVal);

    for (const auto& obj : m_GameObjects)
        json_array_append_value(arr, obj->Serialize());

    json_object_set_string(rootObj, "name", m_Name.c_str());
    json_object_set_number(rootObj, "version", 0.1);
    json_object_set_value(rootObj, "GameObjects", arrVal);

    json_serialize_to_file_pretty(root, filepath.c_str());
    json_value_free(root);

    NOUS_INFO("Scene saved: %s with %zu objects", filepath.c_str(), m_GameObjects.size());
}

// -----------------------------------------------------------------------------
// Deserialization
// -----------------------------------------------------------------------------
void Scene::Deserialize(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    Clear();

    JSON_Value* root = json_parse_file(filepath.c_str());
    if (!root) {
        NOUS_ERROR("Failed to parse scene file: %s", filepath.c_str());
        return;
    }

    JSON_Object* rootObj = json_value_get_object(root);
    const char* sceneName = json_object_get_string(rootObj, "name");
    if (sceneName) m_Name = sceneName;

    JSON_Array* arr = json_object_get_array(rootObj, "GameObjects");
    if (!arr) {
        json_value_free(root);
        NOUS_WARN("No GameObjects array found in scene file");
        return;
    }

    size_t count = json_array_get_count(arr);
    std::vector<std::pair<std::unique_ptr<GameObject>, uint32_t>> gameObjectsWithParents;

    for (size_t i = 0; i < count; ++i) {
        JSON_Object* obj = json_array_get_object(arr, i);
        auto gameObject = GameObject::Deserialize(obj);
        if (gameObject) {
            uint32_t parentID = gameObject->GetParentID();
            gameObjectsWithParents.emplace_back(std::move(gameObject), parentID);
        }
    }

    for (auto& [gameObject, parentID] : gameObjectsWithParents)
        m_GameObjects.push_back(std::move(gameObject));

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
                NOUS_WARN("Parent with ID %u not found for %s", parentID, gameObject->GetName().c_str());
            }
        }
    }

    json_value_free(root);
    NOUS_WARN("[%s] Successfully loaded scene: %s with %zu objects", __FUNCTION__, filepath.c_str(), m_GameObjects.size());
}

// -----------------------------------------------------------------------------
// Clear
// -----------------------------------------------------------------------------
void Scene::Clear() {
    for (auto& gameObject : m_GameObjects) {
        auto children = gameObject->GetChildren();
        for (auto* child : children)
            gameObject->RemoveChild(child);

        gameObject->SetParent(nullptr);
    }
    m_GameObjects.clear();
}

// -----------------------------------------------------------------------------
// Private Helpers
// -----------------------------------------------------------------------------
void Scene::CollectGameObjectTree(GameObject* root, std::vector<GameObject*>& collection) {
    if (!root) return;

    std::queue<GameObject*> queue;
    queue.push(root);

    while (!queue.empty()) {
        GameObject* current = queue.front();
        queue.pop();

        if (std::find(collection.begin(), collection.end(), current) == collection.end()) {
            collection.push_back(current);
            for (GameObject* child : current->GetChildren())
                queue.push(child);
        }
    }
}

void Scene::DestroySingleGameObject(GameObject* go) {
    if (!go) return;

    if (External->scene->selectedGameObject == go)
        External->scene->selectedGameObject = nullptr;

    if (auto* parent = go->GetParent())
        parent->RemoveChild(go);

    for (auto* child : go->GetChildren())
        go->RemoveChild(child);

    auto it = std::find_if(m_GameObjects.begin(), m_GameObjects.end(),
                           [go](const std::unique_ptr<GameObject>& obj) {
                               return obj.get() == go;
                           });
    if (it != m_GameObjects.end())
        m_GameObjects.erase(it);
}

GameObject* Scene::FindGameObjectByID_NoLock(uint32_t id) {
    auto it = std::find_if(m_GameObjects.begin(), m_GameObjects.end(),
                           [id](const std::unique_ptr<GameObject>& obj) {
                               return obj->GetID() == id;
                           });
    return it != m_GameObjects.end() ? it->get() : nullptr;
}

uint32_t Scene::GenerateUniqueID() {
    uint32_t id;
    do {
        id = static_cast<uint32_t>(Random::Generate());
    } while (FindGameObjectByID_NoLock(id) != nullptr);
    return id;
}
