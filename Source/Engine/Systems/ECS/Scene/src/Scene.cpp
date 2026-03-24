#include "Engine/Systems/ECS/Scene/include/Scene.h"

#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/ECS/Component/CTransform/include/CTransform.h"
#include "Engine/Core/Application.h"
#include "Engine/Modules/ModuleScene/include/ModuleScene.h"
#include "Engine/Utils/Serialization/Random/Random.h"
#include "Engine/Core/Logger/Logger.h"

#include <queue>
#include <parson.h>

// -----------------------------------------------------------------------------
// Constructor / Destructor
// -----------------------------------------------------------------------------
Scene::Scene(const std::string& name)
        : m_Name(name), m_GameObjects(MemoryTag::GAMEOBJECT) {}

Scene::~Scene() {
    Clear();
}

// -----------------------------------------------------------------------------
// GameObject Creation & Destruction
// -----------------------------------------------------------------------------
GameObject* Scene::CreateGameObject(const std::string& name, GameObject* parent) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    auto id = GenerateUniqueID();
    auto* go = NOUS_NEW<GameObject>(MemoryTag::GAMEOBJECT, id, name);

    go->AddComponent<CTransform>();

    if (parent)
    {
        parent->AddChild(go);
    }

    m_GameObjects.push_back(go);
    return go;
}

GameObject* Scene::CreateGameObjectDetached(const std::string& name, GameObject* parent)
{
    uint32_t id;
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        id = GenerateUniqueID();
    }

    auto* go = NOUS_NEW<GameObject>(MemoryTag::GAMEOBJECT, id, name);
    go->AddComponent<CTransform>();

    if (parent)
        parent->AddChild(go);

    return go;
}

void Scene::RegisterGameObject(GameObject* go)
{
    if (!go) return;
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_GameObjects.push_back(go);
}

void Scene::DestroyGameObject(GameObject* go) {
    if (!go) return;

    NOUS_Vector<GameObject*> toDestroy(MemoryTag::GAMEOBJECT);
    CollectGameObjectTree(go, toDestroy);

    std::lock_guard<std::mutex> lock(m_Mutex);

    for (auto it = toDestroy.rbegin(); it != toDestroy.rend(); ++it)
    {
        DestroySingleGameObject(*it);
    }
}

// -----------------------------------------------------------------------------
// World matrix update (top-down hierarchy pass)
// -----------------------------------------------------------------------------
static void UpdateWorldMatrixRecursive(GameObject* go)
{
    if (auto* t = go->TryGetComponent<CTransform>())
        t->UpdateMatrix();

    for (auto* child : go->GetChildren())
        UpdateWorldMatrixRecursive(child);
}

void Scene::UpdateWorldMatrices()
{
    // Snapshot so job-thread mutations to m_GameObjects don't race.
    NOUS_Vector<GameObject*> snapshot(MemoryTag::GAMEOBJECT);
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        snapshot = m_GameObjects;
    }

    // Only start the recursion from root objects (no parent).
    // Children are visited by UpdateWorldMatrixRecursive so they get parent's
    // worldMatrix before their own UpdateMatrix() is called.
    for (auto* go : snapshot)
    {
        if (!go->GetParent())
            UpdateWorldMatrixRecursive(go);
    }
}

// -----------------------------------------------------------------------------
// Update
// -----------------------------------------------------------------------------
void Scene::Update(float deltaTime) {
    // Snapshot under lock so job-thread mutations to m_GameObjects are safe,
    // then release the lock before dispatching UpdateComponents.
    // This allows scripts to call back into scene queries (GetGameObjectByID etc.)
    // without deadlocking on m_Mutex.
    NOUS_Vector<GameObject*> snapshot(MemoryTag::GAMEOBJECT);
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        snapshot = m_GameObjects;
    }

    for (auto& gameObject : snapshot) {
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

NOUS_Vector<GameObject*> Scene::FindGameObjectsByName(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    NOUS_Vector<GameObject*> result(MemoryTag::GAMEOBJECT);
    for (auto& gameObject : m_GameObjects) {
        if (gameObject->GetName() == name) {
            result.push_back(gameObject);
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

NOUS_Vector<GameObject*>& Scene::GetGameObjects() { return m_GameObjects; }

NOUS_Vector<GameObject*> Scene::GetGameObjectsSnapshot() const
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_GameObjects;
}

// -----------------------------------------------------------------------------
// Serialization
// -----------------------------------------------------------------------------
void Scene::Serialize(const std::string& filepath) const
{
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
void Scene::Deserialize(const std::string& filepath)
{
    // Phase 1 — parse JSON and build GameObjects into a local vector.
    // No mutex held here so the main thread can call GetGameObjectsSnapshot()
    // freely (it will simply get an empty vector while loading is in progress).
    JSON_Value* root = json_parse_file(filepath.c_str());
    if (!root) {
        NOUS_ERROR("Failed to parse scene file: %s", filepath.c_str());
        return;
    }

    JSON_Object* rootObj = json_value_get_object(root);
    const char* sceneName = json_object_get_string(rootObj, "name");
    std::string newName = sceneName ? sceneName : "";

    JSON_Array* arr = json_object_get_array(rootObj, "GameObjects");
    if (!arr) {
        json_value_free(root);
        NOUS_WARN("No GameObjects array found in scene file");
        return;
    }

    size_t count = json_array_get_count(arr);
    NOUS_Vector<std::pair<GameObject*, uint32_t>> gameObjectsWithParents(MemoryTag::GAMEOBJECT);

    for (size_t i = 0; i < count; ++i) {
        JSON_Object* obj = json_array_get_object(arr, i);
        auto* gameObject = GameObject::Deserialize(obj);
        if (gameObject)
            gameObjectsWithParents.push_back({gameObject, gameObject->GetParentID()});
    }

    json_value_free(root);

    // Phase 3 — batch-commit to m_GameObjects and wire parent relationships.
    // Lock is held only for this brief write phase, not during JSON parsing.
    {
        std::lock_guard<std::mutex> lock(m_Mutex);

        if (!newName.empty()) m_Name = newName;

        for (auto& [gameObject, _] : gameObjectsWithParents)
            m_GameObjects.push_back(gameObject);

        for (auto& gameObject : m_GameObjects) {
            uint32_t parentID = gameObject->GetParentID();
            if (parentID != 0) {
                GameObject* parent = FindGameObjectByID_NoLock(parentID);
                if (parent) {
                    parent->AddChild(gameObject);
                    NOUS_INFO("Set parent: %s (ID: %u) -> %s (ID: %u)",
                              gameObject->GetName().c_str(), gameObject->GetID(),
                              parent->GetName().c_str(), parentID);
                } else {
                    NOUS_WARN("Parent with ID %u not found for %s", parentID, gameObject->GetName().c_str());
                }
            }
        }
    }

    NOUS_DEBUG("Successfully loaded scene: %s with %zu objects", filepath.c_str(), gameObjectsWithParents.size());
}

// -----------------------------------------------------------------------------
// Clear
// -----------------------------------------------------------------------------
void Scene::Clear()
{
    for (GameObject* go : m_GameObjects)
    {
        auto children = go->GetChildren();
        for (auto* child : children)
            go->RemoveChild(child);

        go->SetParent(nullptr);

        NOUS_DELETE(go, MemoryTag::GAMEOBJECT);
    }

    m_GameObjects.clear();
}

// -----------------------------------------------------------------------------
// Private Helpers
// -----------------------------------------------------------------------------
void Scene::CollectGameObjectTree(GameObject* root, NOUS_Vector<GameObject*>& collection) {
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

    if (External && External->scene && External->scene->selectedGameObject == go)
        External->scene->selectedGameObject = nullptr;

    if (auto* parent = go->GetParent())
        parent->RemoveChild(go);

    for (auto* child : go->GetChildren())
        go->RemoveChild(child);

    auto it = std::find(m_GameObjects.begin(), m_GameObjects.end(), go);
    if (it != m_GameObjects.end())
    {
        m_GameObjects.EraseAt(it - m_GameObjects.begin());
    }
    NOUS_DELETE(go, MemoryTag::GAMEOBJECT);
}

GameObject* Scene::FindGameObjectByID_NoLock(uint32_t id) {
    auto it = std::find_if(m_GameObjects.begin(), m_GameObjects.end(),
                           [id](const GameObject* obj) {
                               return obj->GetID() == id;
                           });
    return it != m_GameObjects.end() ? *it : nullptr;
}

uint32_t Scene::GenerateUniqueID() {
    uint32_t id;
    do {
        id = static_cast<uint32_t>(Random::Generate());
    } while (id == 0 || FindGameObjectByID_NoLock(id) != nullptr);
    return id;
}
