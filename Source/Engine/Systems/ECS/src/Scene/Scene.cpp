#include <ECS/Scene/Scene.h>

#ifdef _PROFILING
#include <tracy/Tracy.hpp>
#endif
#include <ECS/GameObject.h>
#include <ECS/Component/Types/CTransform/CTransform.h>
#include <ECS/Component/Types/CMesh/CMesh.h>
#include <ECS/Component/Types/CMaterial/CMaterial.h>
#include <ECS/Component/Types/CCamera/CCamera.h>
#include <ResourceManager/Types/ResourceMaterial/ResourceMaterial.h>
#include <ResourceManager/Types/ResourceMaterial/ImporterMaterial.h>
#include <ECS/Component/Types/CLight/CLight.h>
#include <ECS/Component/Types/CScript/CScript.h>
#include <ECS/Component/Types/CPrefab/CPrefab.h>
#include <ECS/Component/Types/ComponentTypes.h>
#include <PrefabManager/PrefabManager.h>
#include <Utils/Serialization/Random.h>
#include <Logger/Asserts.h>
#include <Logger/Logger.h>

#include <Utils/Serialization/JsonFile.h>
#include <Utils/Serialization/JsonArray.h>
#include <functional>
#include <queue>

// ── Constructor / Destructor ──────────────────────────────────────────────────

Scene::Scene(const std::string& name, const ComponentServices* services)
    : m_name(name), m_services(services)
{
    m_registry.ctx().emplace<Scene*>(this);
    // Same channel that already backs GameObject::GetScene(). Component::Services()
    // reads it back; a null pointer here degrades to the empty aggregate, not a throw.
    m_registry.ctx().emplace<const ComponentServices*>(m_services);
}

const ComponentServices& Scene::GetServices() const {
    static const ComponentServices s_empty;
    return m_services ? *m_services : s_empty;
}

Scene::~Scene() {
    Clear();
}

// ── GameObject Creation ───────────────────────────────────────────────────────

GameObject Scene::CreateGameObject(const std::string& name, GameObject* parent) {
    std::lock_guard lock(m_mutex);

    const uint32_t     id     = GenerateUniqueID();
    const entt::entity entity = m_registry.create();

    m_registry.emplace<CEntityInfo>(entity, id, name);
    m_registry.emplace<CHierarchy>(entity);
    m_idToEntity[id] = entity;
    m_orderedEntities.push_back(entity);

    GameObject go(entity, &m_registry);
    go.AddComponent<CTransform>();

    if (parent && parent->IsValid()) {
        CHierarchy& parentH = m_registry.get<CHierarchy>(parent->GetEntity());
        parentH.children.push_back(entity);
        m_registry.get<CHierarchy>(entity).parent = parent->GetEntity();
    }

    return go;
}

GameObject Scene::CreateGameObjectDetached(const std::string& name, GameObject* parent, uint32_t preferredUID) {
    NOUS_ASSERT_MAIN_THREAD();

    uint32_t id;
    {
        std::lock_guard lock(m_mutex);
        if (preferredUID != 0 && !m_idToEntity.count(preferredUID))
            id = preferredUID;
        else
            id = GenerateUniqueID();
    }

    const entt::entity entity = m_registry.create();
    m_registry.emplace<CEntityInfo>(entity, id, name);
    m_registry.emplace<CHierarchy>(entity);

    {
        std::lock_guard lock(m_mutex);
        m_idToEntity[id] = entity;
    }

    GameObject go(entity, &m_registry);
    go.AddComponent<CTransform>();

    if (parent && parent->IsValid()) {
        CHierarchy& parentH = m_registry.get<CHierarchy>(parent->GetEntity());
        parentH.children.push_back(entity);
        m_registry.get<CHierarchy>(entity).parent = parent->GetEntity();
    }

    return go;
}

void Scene::RegisterGameObject(GameObject go) {
    if (!go.IsValid()) return;
    std::lock_guard lock(m_mutex);
    const uint32_t id = m_registry.get<CEntityInfo>(go.GetEntity()).id;
    m_idToEntity[id]  = go.GetEntity();
    m_orderedEntities.push_back(go.GetEntity());
}

// ── Destruction ───────────────────────────────────────────────────────────────

void Scene::DestroyGameObject(GameObject go) {
    if (!go.IsValid()) return;

    std::vector<entt::entity> toDestroy;
    CollectEntityTree(go.GetEntity(), toDestroy);

    std::lock_guard lock(m_mutex);
    for (auto it = toDestroy.rbegin(); it != toDestroy.rend(); ++it)
        DestroyEntity(*it);
}

// ── Update ────────────────────────────────────────────────────────────────────

void Scene::Update(float deltaTime) {
#ifdef _PROFILING
    ZoneScopedN("Scene::Update");
#endif
    // Tick every component type that does real per-frame work. The set lives in
    // UpdatableComponentTypes (ComponentTypes.h) — adding a per-frame component is a
    // one-line edit there, and we never walk the no-op views (CTransform, CMesh, ...).
    UpdatableComponentTypes::ForEachType([this, deltaTime]<typename T>() {
#ifdef _PROFILING
        ZoneScoped;
        ZoneName(T::TypeName.data(), T::TypeName.size());
#endif
        m_registry.view<T>().each([deltaTime](T& c) { c.OnUpdate(deltaTime); });
    });
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
    for (auto [entity, hierarchy] : m_registry.view<CHierarchy>().each()) {
        if (hierarchy.parent == entt::null)
            UpdateWorldMatrixRecursive(entity, m_registry, false);
    }
}

// ── Lookup ────────────────────────────────────────────────────────────────────

GameObject Scene::FindGameObjectByID(uint32_t id) {
    std::lock_guard lock(m_mutex);
    const entt::entity e = FindEntityByID_NoLock(id);
    if (e == entt::null) return {};
    return GameObject(e, &m_registry);
}

GameObject Scene::FindGameObjectByName(const std::string& name) {
    std::lock_guard lock(m_mutex);
    for (auto [entity, info] : m_registry.view<CEntityInfo>().each()) {
        if (info.name == name)
            return GameObject(entity, &m_registry);
    }
    return {};
}

std::vector<GameObject> Scene::GetGameObjects() const {
    std::lock_guard lock(m_mutex);
    std::vector<GameObject> result;
    result.reserve(m_orderedEntities.size());
    for (auto entity : m_orderedEntities)
        result.emplace_back(entity, const_cast<entt::registry*>(&m_registry));
    return result;
}

// ── Name ─────────────────────────────────────────────────────────────────────

const std::string& Scene::GetName() const { return m_name; }
void Scene::SetName(const std::string& name) { m_name = name; }

// ── Serialization ─────────────────────────────────────────────────────────────

void Scene::Serialize(const std::string& filepath) const {
    // DFS from roots in m_orderedEntities order, following CHierarchy::children
    // at each node. This preserves sibling order across save/load round-trips.
    // Using view<CEntityInfo>() here would serialize in EnTT's internal packed-array
    // order, which differs from CHierarchy::children order and causes siblings to
    // swap on every snapshot restore.
    JsonArray goArr;
    std::function<void(entt::entity)> writeDFS = [&](entt::entity entity) {
        GameObject go(entity, const_cast<entt::registry*>(&m_registry));
        goArr.Append(go.Serialize());
        if (const auto* h = m_registry.try_get<CHierarchy>(entity))
            for (auto child : h->children)
                writeDFS(child);
    };
    for (auto entity : m_orderedEntities) {
        const auto* h = m_registry.try_get<CHierarchy>(entity);
        if (h && h->parent == entt::null)
            writeDFS(entity);
    }

    JsonObject root;
    root.Set("name",        m_name);
    root.Set("version",     0.1);
    root.Set("GameObjects", std::move(goArr));

    JsonFile::SaveToFile(root, filepath);

    m_registry.view<CMaterial>().each([](const CMaterial& c) {
        if (c.material && !c.material->GetAssetsPath().empty())
            ImporterMaterial::SaveMaterialToAssets(c.material);
    });

    NOUS_INFO("Scene saved: %s", filepath.c_str());
}

void Scene::Deserialize(const std::string& filepath) {
    NOUS_ASSERT_MAIN_THREAD();

    JsonObject root = JsonFile::LoadFromFile(filepath);
    if (root.IsEmpty()) {
        NOUS_ERROR("Failed to parse scene file: %s", filepath.c_str());
        return;
    }

    const std::string sceneName = root.GetString("name");
    if (!sceneName.empty()) m_name = sceneName;

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
        if (go.IsValid())
            created.push_back({ go, parentID });
    }

    {
        std::lock_guard lock(m_mutex);
        // Publish the ID map and ordered list under the lock — consistent with
        // CreateGameObjectDetached / RegisterGameObject. A concurrent main-thread
        // FindGameObjectByID (which locks m_mutex) must never read a half-written
        // m_idToEntity. Populate it before the parent-wiring loop below, which
        // resolves parents via FindEntityByID_NoLock.
        for (auto& [go, parentID] : created) {
            m_idToEntity[go.GetID()] = go.GetEntity();
            m_orderedEntities.push_back(go.GetEntity());
        }

        for (auto& [go, parentID] : created) {
            if (parentID == 0) continue;
            const entt::entity parentEntity = FindEntityByID_NoLock(parentID);
            if (parentEntity == entt::null) {
                NOUS_WARN("Parent ID %u not found for %s", parentID, go.GetName().c_str());
                continue;
            }
            m_registry.get<CHierarchy>(go.GetEntity()).parent = parentEntity;
            m_registry.get<CHierarchy>(parentEntity).children.push_back(go.GetEntity());
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
    // later UpdateWorldMatrices() pass picked them up. CTransform::Deserialize ends
    // with MarkDirty(), and only this pass clears m_localDirty, so every transform is
    // still dirty here and gets recomputed with its parent in place.
    UpdateWorldMatrices();

    NOUS_DEBUG("Loaded scene: %s with %d objects", filepath.c_str(), count);
}

// ── Clear ─────────────────────────────────────────────────────────────────────

void Scene::Clear() {
    // Fire OnDestroy for every component of every registered type, in list order.
    ComponentTypes::ForEachType([this]<typename T>() {
        m_registry.view<T>().each([](T& c) { c.OnDestroy(); });
    });

    m_registry.clear();
    m_idToEntity.clear();
    m_orderedEntities.clear();
}

// ── Private helpers ───────────────────────────────────────────────────────────

void Scene::CollectEntityTree(entt::entity root, std::vector<entt::entity>& out) {
    std::queue<entt::entity> q;
    q.push(root);
    while (!q.empty()) {
        entt::entity e = q.front(); q.pop();
        out.push_back(e);
        if (const auto* h = m_registry.try_get<CHierarchy>(e))
            for (auto child : h->children)
                q.push(child);
    }
}

void Scene::DestroyEntity(entt::entity entity) {
    if (!m_registry.valid(entity)) return;

    // NOTE: no selection pruning here. ModuleScene observes on_destroy<CEntityInfo>
    // instead, so Scene does not know that "selection" exists — the module watches
    // the scene rather than the scene announcing itself to the module.

    if (const auto* h = m_registry.try_get<CHierarchy>(entity)) {
        if (h->parent != entt::null) {
            auto& parentH = m_registry.get<CHierarchy>(h->parent);
            parentH.children.erase(
                std::remove(parentH.children.begin(), parentH.children.end(), entity),
                parentH.children.end());
        }
    }

    if (const auto* info = m_registry.try_get<CEntityInfo>(entity)) {
        m_idToEntity.erase(info->id);
        auto it = std::find(m_orderedEntities.begin(), m_orderedEntities.end(), entity);
        if (it != m_orderedEntities.end())
            m_orderedEntities.erase(it);
    }

    // Match Clear() behavior: fire OnDestroy for every component type before
    // the registry destructs the entity. Skipping this leaves ScriptManager
    // holding stale CScript* pointers and leaks GPU resources for CMesh /
    // CMaterial because their OnDestroy() calls (vkDeviceWaitIdle, etc.)
    // never run.
    // Fire OnDestroy for every attached component before the entity is destroyed,
    // in ComponentTypes list order (same order as Clear() — the two paths now agree).
    ComponentTypes::ForEachPresent(m_registry, entity,
        [](Component* c) { c->OnDestroy(); });

    m_registry.destroy(entity);
}

entt::entity Scene::FindEntityByID_NoLock(uint32_t id) const {
    const auto it = m_idToEntity.find(id);
    return it != m_idToEntity.end() ? it->second : entt::null;
}

uint32_t Scene::GenerateUniqueID() {
    uint32_t id;
    do {
        id = static_cast<uint32_t>(Random::Generate());
    } while (id == 0 || m_idToEntity.count(id));
    return id;
}
