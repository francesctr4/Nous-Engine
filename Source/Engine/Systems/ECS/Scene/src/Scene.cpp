#include "Engine/Systems/ECS/Scene/include/Scene.h"

#ifdef _PROFILING
#include <tracy/Tracy.hpp>
#endif
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/ECS/Component/CTransform/include/CTransform.h"
#include "Engine/Systems/ECS/Component/CMesh/include/CMesh.h"
#include "Engine/Systems/ECS/Component/CMaterial/include/CMaterial.h"
#include "Engine/Systems/ECS/Component/CCamera/include/CCamera.h"
#include "Engine/Systems/ECS/Component/CLight/include/CLight.h"
#include "Engine/Systems/ECS/Component/CScript/include/CScript.h"
#include "Engine/Systems/ECS/Component/CPrefab/include/CPrefab.h"
#include "Engine/Modules/ModuleScene/include/ModuleScene.h"
#include "Engine/Utils/Serialization/Random/Random.h"
#include "Engine/Core/Logger/Logger.h"

#include <parson.h>
#include <queue>

// ── Constructor / Destructor ──────────────────────────────────────────────────

Scene::Scene(const std::string& name, ModuleScene* moduleScene, ModuleResourceManager* resourceManager)
    : m_Name(name), m_ModuleScene(moduleScene), m_ResourceManager(resourceManager)
{
    m_Registry.ctx().emplace<Scene*>(this);
}

Scene::~Scene() {
    Clear();
}

// ── GameObject Creation ───────────────────────────────────────────────────────

GameObject Scene::CreateGameObject(const std::string& name, GameObject* parent) {
    std::lock_guard lock(m_Mutex);

    const uint32_t     id     = GenerateUniqueID();
    const entt::entity entity = m_Registry.create();

    m_Registry.emplace<CEntityInfo>(entity, id, name);
    m_Registry.emplace<CHierarchy>(entity);
    m_IDToEntity[id] = entity;

    GameObject go(entity, &m_Registry);
    go.AddComponent<CTransform>();

    if (parent && parent->IsValid()) {
        CHierarchy& parentH = m_Registry.get<CHierarchy>(parent->GetEntity());
        parentH.children.push_back(entity);
        m_Registry.get<CHierarchy>(entity).parent = parent->GetEntity();
    }

    return go;
}

GameObject Scene::CreateGameObjectDetached(const std::string& name, GameObject* parent) {
    uint32_t id;
    {
        std::lock_guard lock(m_Mutex);
        id = GenerateUniqueID();
    }

    const entt::entity entity = m_Registry.create();
    m_Registry.emplace<CEntityInfo>(entity, id, name);
    m_Registry.emplace<CHierarchy>(entity);

    {
        std::lock_guard lock(m_Mutex);
        m_IDToEntity[id] = entity;
    }

    GameObject go(entity, &m_Registry);
    go.AddComponent<CTransform>();

    if (parent && parent->IsValid()) {
        CHierarchy& parentH = m_Registry.get<CHierarchy>(parent->GetEntity());
        parentH.children.push_back(entity);
        m_Registry.get<CHierarchy>(entity).parent = parent->GetEntity();
    }

    return go;
}

void Scene::RegisterGameObject(GameObject go) {
    if (!go.IsValid()) return;
    std::lock_guard lock(m_Mutex);
    const uint32_t id = m_Registry.get<CEntityInfo>(go.GetEntity()).id;
    m_IDToEntity[id]  = go.GetEntity();
}

// ── Destruction ───────────────────────────────────────────────────────────────

void Scene::DestroyGameObject(GameObject go) {
    if (!go.IsValid()) return;

    std::vector<entt::entity> toDestroy;
    CollectEntityTree(go.GetEntity(), toDestroy);

    std::lock_guard lock(m_Mutex);
    for (auto it = toDestroy.rbegin(); it != toDestroy.rend(); ++it)
        DestroyEntity(*it);
}

// ── Update ────────────────────────────────────────────────────────────────────

void Scene::Update(float deltaTime) {
#ifdef _PROFILING
    ZoneScopedN("Scene::Update");
#endif
    {
#ifdef _PROFILING
        ZoneScopedN("CScript::OnUpdate");
#endif
        m_Registry.view<CScript>().each([deltaTime](CScript& s) { s.OnUpdate(deltaTime); });
    }
    {
#ifdef _PROFILING
        ZoneScopedN("CCamera::OnUpdate");
#endif
        m_Registry.view<CCamera>().each([deltaTime](CCamera& c) { c.OnUpdate(deltaTime); });
    }
    {
#ifdef _PROFILING
        ZoneScopedN("CLight::OnUpdate");
#endif
        m_Registry.view<CLight>().each([deltaTime](CLight& l) { l.OnUpdate(deltaTime); });
    }
}

// ── World Matrix Update ───────────────────────────────────────────────────────

static void UpdateWorldMatrixRecursive(entt::entity entity, entt::registry& registry, bool parentWasDirty) {
    auto* t = registry.try_get<CTransform>(entity);
    const bool isDirty = parentWasDirty || (t && t->m_localDirty);

    if (t) {
        if (isDirty) {
            t->UpdateMatrix();
            t->m_localDirty = false;
            t->m_worldDirty = true;
        } else {
            t->m_worldDirty = false;
        }
    }

    if (const auto* h = registry.try_get<CHierarchy>(entity))
        for (auto child : h->children)
            UpdateWorldMatrixRecursive(child, registry, isDirty);
}

void Scene::UpdateWorldMatrices() {
    for (auto [entity, hierarchy] : m_Registry.view<CHierarchy>().each()) {
        if (hierarchy.parent == entt::null)
            UpdateWorldMatrixRecursive(entity, m_Registry, false);
    }
}

// ── Lookup ────────────────────────────────────────────────────────────────────

GameObject Scene::FindGameObjectByID(uint32_t id) {
    std::lock_guard lock(m_Mutex);
    const entt::entity e = FindEntityByID_NoLock(id);
    if (e == entt::null) return {};
    return GameObject(e, &m_Registry);
}

GameObject Scene::GetGameObjectByID(uint32_t id) {
    return FindGameObjectByID(id);
}

std::vector<GameObject> Scene::GetGameObjects() const {
    std::lock_guard lock(m_Mutex);
    std::vector<GameObject> result;
    for (auto entity : m_Registry.view<CEntityInfo>())
        result.emplace_back(entity, const_cast<entt::registry*>(&m_Registry));
    return result;
}

std::vector<GameObject> Scene::GetGameObjectsSnapshot() const {
    return GetGameObjects();
}

// ── Name ─────────────────────────────────────────────────────────────────────

const std::string& Scene::GetName() const { return m_Name; }
void Scene::SetName(const std::string& name) { m_Name = name; }

// ── Serialization ─────────────────────────────────────────────────────────────

void Scene::Serialize(const std::string& filepath) const {
    JSON_Value*  root    = json_value_init_object();
    JSON_Object* rootObj = json_value_get_object(root);
    JSON_Value*  arrVal  = json_value_init_array();
    JSON_Array*  arr     = json_value_get_array(arrVal);

    for (auto entity : m_Registry.view<CEntityInfo>()) {
        GameObject go(entity, const_cast<entt::registry*>(&m_Registry));
        if (JSON_Value* v = go.Serialize())
            json_array_append_value(arr, v);
    }

    json_object_set_string(rootObj, "name",    m_Name.c_str());
    json_object_set_number(rootObj, "version", 0.1);
    json_object_set_value(rootObj,  "GameObjects", arrVal);

    json_serialize_to_file_pretty(root, filepath.c_str());
    json_value_free(root);

    NOUS_INFO("Scene saved: %s", filepath.c_str());
}

void Scene::Deserialize(const std::string& filepath) {
    JSON_Value* root = json_parse_file(filepath.c_str());
    if (!root) {
        NOUS_ERROR("Failed to parse scene file: %s", filepath.c_str());
        return;
    }

    JSON_Object* rootObj  = json_value_get_object(root);
    const char*  sceneName = json_object_get_string(rootObj, "name");
    if (sceneName) m_Name = sceneName;

    JSON_Array* arr = json_object_get_array(rootObj, "GameObjects");
    if (!arr) {
        json_value_free(root);
        NOUS_WARN("No GameObjects array found in scene file");
        return;
    }

    const size_t count = json_array_get_count(arr);
    std::vector<std::pair<GameObject, uint32_t>> created;
    // Iterate JSON in reverse: EnTT views traverse the packed array in reverse
    // insertion order, so serialize already wrote entities in reverse-of-creation
    // order. Emplacing them back in reverse-of-JSON order rebuilds the packed
    // array so the next view iteration matches the JSON order — making save/load
    // round-trips stable. Without this, sibling order flips on every save.
    for (size_t i = count; i-- > 0;) {
        JSON_Object* obj = json_array_get_object(arr, i);
        // Read parentID from JSON BEFORE deserializing — GameObject::Deserialize()
        // does not store it in CHierarchy (that wiring happens below), so calling
        // go.GetParentID() afterward would always return 0.
        const auto parentID = static_cast<uint32_t>(json_object_get_number(obj, "parent"));
        GameObject go = GameObject::Deserialize(obj, this);
        if (go.IsValid()) {
            m_IDToEntity[go.GetID()] = go.GetEntity();
            created.push_back({ go, parentID });
        }
    }

    json_value_free(root);

    {
        std::lock_guard lock(m_Mutex);
        for (auto& [go, parentID] : created) {
            if (parentID == 0) continue;
            const entt::entity parentEntity = FindEntityByID_NoLock(parentID);
            if (parentEntity == entt::null) {
                NOUS_WARN("Parent ID %u not found for %s", parentID, go.GetName().c_str());
                continue;
            }
            m_Registry.get<CHierarchy>(go.GetEntity()).parent = parentEntity;
            m_Registry.get<CHierarchy>(parentEntity).children.push_back(go.GetEntity());
        }
    }

    NOUS_DEBUG("Loaded scene: %s with %zu objects", filepath.c_str(), created.size());
}

// ── Clear ─────────────────────────────────────────────────────────────────────

void Scene::Clear() {
    m_Registry.view<CTransform>().each([](CTransform& c) { c.OnDestroy(); });
    m_Registry.view<CMesh>().each     ([](CMesh&      c) { c.OnDestroy(); });
    m_Registry.view<CMaterial>().each ([](CMaterial&  c) { c.OnDestroy(); });
    m_Registry.view<CCamera>().each   ([](CCamera&    c) { c.OnDestroy(); });
    m_Registry.view<CLight>().each    ([](CLight&     c) { c.OnDestroy(); });
    m_Registry.view<CScript>().each   ([](CScript&    c) { c.OnDestroy(); });
    m_Registry.view<CPrefab>().each   ([](CPrefab&    c) { c.OnDestroy(); });

    m_Registry.clear();
    m_IDToEntity.clear();
}

// ── Private helpers ───────────────────────────────────────────────────────────

void Scene::CollectEntityTree(entt::entity root, std::vector<entt::entity>& out) {
    std::queue<entt::entity> q;
    q.push(root);
    while (!q.empty()) {
        entt::entity e = q.front(); q.pop();
        out.push_back(e);
        if (const auto* h = m_Registry.try_get<CHierarchy>(e))
            for (auto child : h->children)
                q.push(child);
    }
}

void Scene::DestroyEntity(entt::entity entity) {
    if (!m_Registry.valid(entity)) return;

    if (m_ModuleScene) {
        GameObject go(entity, &m_Registry);
        if (m_ModuleScene->selectedGameObject == go)
            m_ModuleScene->selectedGameObject = {};
    }

    if (const auto* h = m_Registry.try_get<CHierarchy>(entity)) {
        if (h->parent != entt::null) {
            auto& parentH = m_Registry.get<CHierarchy>(h->parent);
            parentH.children.erase(
                std::remove(parentH.children.begin(), parentH.children.end(), entity),
                parentH.children.end());
        }
    }

    if (const auto* info = m_Registry.try_get<CEntityInfo>(entity))
        m_IDToEntity.erase(info->id);

    // Match Clear() behavior: fire OnDestroy for every component type before
    // the registry destructs the entity. Skipping this leaves ScriptManager
    // holding stale CScript* pointers and leaks GPU resources for CMesh /
    // CMaterial because their OnDestroy() calls (vkDeviceWaitIdle, etc.)
    // never run.
    if (auto* c = m_Registry.try_get<CScript>   (entity)) c->OnDestroy();
    if (auto* c = m_Registry.try_get<CMaterial> (entity)) c->OnDestroy();
    if (auto* c = m_Registry.try_get<CMesh>     (entity)) c->OnDestroy();
    if (auto* c = m_Registry.try_get<CCamera>   (entity)) c->OnDestroy();
    if (auto* c = m_Registry.try_get<CLight>    (entity)) c->OnDestroy();
    if (auto* c = m_Registry.try_get<CTransform>(entity)) c->OnDestroy();
    if (auto* c = m_Registry.try_get<CPrefab>   (entity)) c->OnDestroy();

    m_Registry.destroy(entity);
}

entt::entity Scene::FindEntityByID_NoLock(uint32_t id) const {
    const auto it = m_IDToEntity.find(id);
    return it != m_IDToEntity.end() ? it->second : entt::null;
}

uint32_t Scene::GenerateUniqueID() {
    uint32_t id;
    do {
        id = static_cast<uint32_t>(Random::Generate());
    } while (id == 0 || m_IDToEntity.count(id));
    return id;
}
