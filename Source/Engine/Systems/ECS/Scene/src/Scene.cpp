#include "Engine/Systems/ECS/Scene/include/Scene.h"

#ifdef _PROFILING
#include <tracy/Tracy.hpp>
#endif
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/ECS/Component/CTransform/include/CTransform.h"
#include "Engine/Systems/ECS/Component/CMesh/include/CMesh.h"
#include "Engine/Systems/ECS/Component/CMaterial/include/CMaterial.h"
#include "Engine/Systems/ECS/Component/CCamera/include/CCamera.h"
#include "Engine/Systems/ResourceManager/Types/ResourceMaterial/include/ResourceMaterial.h"
#include "Engine/Systems/ResourceManager/Types/ResourceMaterial/include/ImporterMaterial.h"
#include "Engine/Systems/ECS/Component/CLight/include/CLight.h"
#include "Engine/Systems/ECS/Component/CScript/include/CScript.h"
#include "Engine/Systems/ECS/Component/CPrefab/include/CPrefab.h"
#include "Engine/Systems/ECS/Component/ComponentTypes.h"
#include "Engine/Systems/PrefabManager/include/PrefabManager.h"
#include "Engine/Modules/ModuleScene/include/ModuleScene.h"
#include "Engine/Utils/Serialization/Random/Random.h"
#include "Engine/Core/Logger/Logger.h"

#include "Engine/Utils/Serialization/JsonFile/JsonFile.h"
#include "Engine/Utils/Serialization/JsonFile/JsonArray.h"
#include <functional>
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
    m_OrderedEntities.push_back(entity);

    GameObject go(entity, &m_Registry);
    go.AddComponent<CTransform>();

    if (parent && parent->IsValid()) {
        CHierarchy& parentH = m_Registry.get<CHierarchy>(parent->GetEntity());
        parentH.children.push_back(entity);
        m_Registry.get<CHierarchy>(entity).parent = parent->GetEntity();
    }

    return go;
}

GameObject Scene::CreateGameObjectDetached(const std::string& name, GameObject* parent, uint32_t preferredUID) {
    uint32_t id;
    {
        std::lock_guard lock(m_Mutex);
        if (preferredUID != 0 && !m_IDToEntity.count(preferredUID))
            id = preferredUID;
        else
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
    m_OrderedEntities.push_back(go.GetEntity());
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

void Scene::UpdateWorldMatrices(bool force) {
    // Passing `force` as the initial parentWasDirty propagates isDirty=true through
    // every descendant, guaranteeing each transform's UpdateMatrix() runs even if
    // m_localDirty was already cleared by a racing pass (e.g. main thread ran this
    // mid-Deserialize on the worker thread, before parent wiring was complete).
    for (auto [entity, hierarchy] : m_Registry.view<CHierarchy>().each()) {
        if (hierarchy.parent == entt::null)
            UpdateWorldMatrixRecursive(entity, m_Registry, force);
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

GameObject Scene::FindGameObjectByName(const std::string& name) {
    std::lock_guard lock(m_Mutex);
    for (auto [entity, info] : m_Registry.view<CEntityInfo>().each()) {
        if (info.name == name)
            return GameObject(entity, &m_Registry);
    }
    return {};
}

std::vector<GameObject> Scene::GetGameObjects() const {
    std::lock_guard lock(m_Mutex);
    std::vector<GameObject> result;
    result.reserve(m_OrderedEntities.size());
    for (auto entity : m_OrderedEntities)
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
    // DFS from roots in m_OrderedEntities order, following CHierarchy::children
    // at each node. This preserves sibling order across save/load round-trips.
    // Using view<CEntityInfo>() here would serialize in EnTT's internal packed-array
    // order, which differs from CHierarchy::children order and causes siblings to
    // swap on every snapshot restore.
    JsonArray goArr;
    std::function<void(entt::entity)> writeDFS = [&](entt::entity entity) {
        GameObject go(entity, const_cast<entt::registry*>(&m_Registry));
        goArr.Append(go.Serialize());
        if (const auto* h = m_Registry.try_get<CHierarchy>(entity))
            for (auto child : h->children)
                writeDFS(child);
    };
    for (auto entity : m_OrderedEntities) {
        const auto* h = m_Registry.try_get<CHierarchy>(entity);
        if (h && h->parent == entt::null)
            writeDFS(entity);
    }

    JsonObject root;
    root.Set("name",        m_Name);
    root.Set("version",     0.1);
    root.Set("GameObjects", std::move(goArr));

    JsonFile::SaveToFile(root, filepath);

    m_Registry.view<CMaterial>().each([](const CMaterial& c) {
        if (c.material && !c.material->GetAssetsPath().empty())
            ImporterMaterial::SaveMaterialToAssets(c.material);
    });

    NOUS_INFO("Scene saved: %s", filepath.c_str());
}

void Scene::Deserialize(const std::string& filepath) {
    JsonObject root = JsonFile::LoadFromFile(filepath);
    if (root.IsEmpty()) {
        NOUS_ERROR("Failed to parse scene file: %s", filepath.c_str());
        return;
    }

    const std::string sceneName = root.GetString("name");
    if (!sceneName.empty()) m_Name = sceneName;

    JsonArray arr = root.GetArray("GameObjects");
    if (arr.IsEmpty()) {
        NOUS_WARN("No GameObjects array found in scene file");
        return;
    }

    const int count = arr.Count();
    std::vector<std::pair<GameObject, uint32_t>> created;
    // Forward iteration: Serialize now writes in DFS tree order (roots first,
    // children in CHierarchy::children order). Iterating forward here preserves
    // that order in `created`, so the parent-wiring loop below adds children to
    // their parent's vector in the correct sibling order.
    for (int i = 0; i < count; ++i) {
        JsonObject obj = arr.GetObject(i);
        if (obj.IsEmpty()) continue;
        // Read parentID from JSON BEFORE deserializing — GameObject::Deserialize()
        // does not store it in CHierarchy (that wiring happens below), so calling
        // go.GetParentID() afterward would always return 0.
        const auto parentID = static_cast<uint32_t>(obj.GetDouble("parent", 0.0));
        GameObject go = GameObject::Deserialize(obj, this);
        if (go.IsValid()) {
            m_IDToEntity[go.GetID()] = go.GetEntity();
            created.push_back({ go, parentID });
        }
    }

    {
        std::lock_guard lock(m_Mutex);
        for (auto& [go, parentID] : created)
            m_OrderedEntities.push_back(go.GetEntity());

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

    for (auto& [go, parentID] : created)
    {
        if (!go.IsValid()) continue;
        // Only rebuild prefab children when the scene file predates inline serialization
        // (i.e. the root was saved without its children). If children are already present
        // they were deserialized from the scene file and must not be replaced — doing so
        // would assign fresh random UIDs every load/save cycle.
        if (go.TryGetComponent<CPrefab>() && go.GetChildren().empty())
            PrefabManager::ReloadPrefabInstance(go, this);
    }

    // Recompute world matrices in tree order now that parent-child wiring is in place.
    // CTransform::Deserialize ran during the first loop above, but at that point each
    // GO's CHierarchy.parent was still entt::null (wiring happens in the second loop),
    // so the worldMatrix it computed was the local matrix only — children of a parent
    // with non-identity transform would render at the wrong position/scale until a
    // later UpdateWorldMatrices() pass picked them up.
    //
    // force=true: when loaded via LoadSceneAsync, this runs on a worker thread while
    // ModuleScene::PostUpdate (main thread) is also calling UpdateWorldMatrices() each
    // frame. That main-thread pass can race in *before* parent wiring completes here,
    // compute wrong worldMatrices from local-only data, and then clear m_localDirty.
    // Without force=true, this final pass would then see m_localDirty=false and skip
    // the recompute, leaving children at the wrong scale. Forcing makes us correct
    // regardless of the racing pre-state.
    UpdateWorldMatrices(true);

    NOUS_DEBUG("Loaded scene: %s with %d objects", filepath.c_str(), count);
}

// ── Clear ─────────────────────────────────────────────────────────────────────

void Scene::Clear() {
    // Fire OnDestroy for every component of every registered type, in list order.
    ComponentTypes::ForEachType([this]<typename T>() {
        m_Registry.view<T>().each([](T& c) { c.OnDestroy(); });
    });

    m_Registry.clear();
    m_IDToEntity.clear();
    m_OrderedEntities.clear();
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
        m_ModuleScene->RemoveFromSelection(go);
    }

    if (const auto* h = m_Registry.try_get<CHierarchy>(entity)) {
        if (h->parent != entt::null) {
            auto& parentH = m_Registry.get<CHierarchy>(h->parent);
            parentH.children.erase(
                std::remove(parentH.children.begin(), parentH.children.end(), entity),
                parentH.children.end());
        }
    }

    if (const auto* info = m_Registry.try_get<CEntityInfo>(entity)) {
        m_IDToEntity.erase(info->id);
        auto it = std::find(m_OrderedEntities.begin(), m_OrderedEntities.end(), entity);
        if (it != m_OrderedEntities.end())
            m_OrderedEntities.erase(it);
    }

    // Match Clear() behavior: fire OnDestroy for every component type before
    // the registry destructs the entity. Skipping this leaves ScriptManager
    // holding stale CScript* pointers and leaks GPU resources for CMesh /
    // CMaterial because their OnDestroy() calls (vkDeviceWaitIdle, etc.)
    // never run.
    // Fire OnDestroy for every attached component before the entity is destroyed,
    // in ComponentTypes list order (same order as Clear() — the two paths now agree).
    ComponentTypes::ForEachPresent(m_Registry, entity,
        [](Component* c) { c->OnDestroy(); });

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
