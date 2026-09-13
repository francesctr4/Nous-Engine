#include <PrefabManager/PrefabManager.h>

#include <ECS/Scene/Scene.h>
#include <ECS/GameObject.h>
#include <ECS/Component/Component.h>
#include <ECS/Component/Types/CPrefab/CPrefab.h>
#include <ECS/Component/Types/CPrefabLink/CPrefabLink.h>
#include <ECS/Component/Types/CTransform/CTransform.h>
#include <ECS/Component/Types/CMesh/CMesh.h>
#include <ECS/Component/Types/CMaterial/CMaterial.h>
#include <ResourceManager/Types/ResourceMaterial/ResourceMaterial.h>
#include <ResourceManager/Types/ResourceMaterial/ImporterMaterial.h>
#include <ECS/Component/Types/CCamera/CCamera.h>
#include <ECS/Component/Types/CLight/CLight.h>
#include <ECS/Component/Types/CScript/CScript.h>
#include <ECS/Component/Types/ComponentTypes.h>
#include <Logger/Logger.h>

#include <Utils/Serialization/JsonFile.h>
#include <Utils/Serialization/JsonArray.h>
#include <filesystem>
#include <fstream>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>

constexpr LogChannel CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_CORE_MODULE_SCENE;

// Adds a component of the given type to go and deserializes it from compObj.
// Delegates the type dispatch to ComponentTypes (shared with GameObject::Deserialize).
static void DeserializeComponentInto(GameObject& go, std::string_view typeName, const JsonObject& compObj)
{
    if (Component* c = ComponentTypes::AddByName(go, typeName))
        c->Deserialize(compObj);
    else
        NOUS_WARN_C(CURRENT_CHANNEL, "[PrefabManager] Unknown component type: %.*s",
                    static_cast<int>(typeName.size()), typeName.data());
}

// Removes a component by type name. The set of removable types is restricted at
// the call site (CTransform/CPrefab/CScript are filtered out before calling here).
static void RemoveComponentByName(GameObject& go, const std::string& typeName)
{
    ComponentTypes::RemoveByName(go, typeName);
}

// Depth-first, root first. Collected BEFORE any mutation, so callers must re-check
// IsValid() on handles they use after destroying anything.
static void CollectSubtree(GameObject root, std::vector<GameObject>& out)
{
    out.push_back(root);
    for (GameObject child : root.GetChildren())
        CollectSubtree(child, out);
}

// -----------------------------------------------------------------------------
// HashPrefabFile
// -----------------------------------------------------------------------------
uint64_t PrefabManager::HashPrefabFile(const std::string& path)
{
    // Binary mode is required: text mode collapses CRLF on Windows, so the same
    // file would hash differently across platforms.
    std::ifstream file(path, std::ios::binary);
    if (!file) return 0;

    // Same constants as HashBoneNames (ImporterSkeleton.cpp).
    constexpr uint64_t c_offsetBasis = 14695981039346656037ull;
    constexpr uint64_t c_prime       = 1099511628211ull;

    uint64_t hash = c_offsetBasis;

    char buffer[4096];
    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0)
    {
        const std::streamsize read = file.gcount();
        for (std::streamsize i = 0; i < read; ++i)
        {
            hash ^= static_cast<uint64_t>(static_cast<unsigned char>(buffer[i]));
            hash *= c_prime;
        }
    }

    return hash;
}

// -----------------------------------------------------------------------------
// SavePrefab
// -----------------------------------------------------------------------------
void PrefabManager::SavePrefab(GameObject root, const std::string& filePath)
{
    if (!root.IsValid())
    {
        NOUS_ERROR("[PrefabManager] SavePrefab called with invalid root handle.");
        return;
    }

    std::filesystem::create_directories(std::filesystem::path(filePath).parent_path());

    // BFS to collect root + all descendants
    std::vector<GameObject> allGOs;
    std::queue<GameObject>  bfsQueue;
    bfsQueue.push(root);
    while (!bfsQueue.empty())
    {
        GameObject current = bfsQueue.front();
        bfsQueue.pop();
        allGOs.push_back(current);
        for (const auto& child : current.GetChildren())
            bfsQueue.push(child);
    }

    JsonArray goArr;
    for (const auto& go : allGOs)
    {
        JsonObject goObj = go.Serialize();

        // The prefab root must always have parent=0 in the file regardless of
        // its position in the scene hierarchy, so ReloadPrefabInstance can
        // reliably identify it.
        if (go == root)
            goObj.Set("parent", 0.0);

        // Strip CPrefab from the saved data — the file IS the prefab definition,
        // so embedding a self-referential CPrefab component is meaningless.
        JsonArray origComps = goObj.GetArray("components");
        if (!origComps.IsEmpty())
        {
            JsonArray newComps;
            for (int ci = 0; ci < origComps.Count(); ++ci)
            {
                JsonObject compObj = origComps.GetObject(ci);

                // The file IS the prefab definition, so neither the instance marker
                // nor a link INTO the file means anything inside it.
                const std::string compType = compObj.GetString("type");
                if (compType == "CPrefab")     continue;
                if (compType == "CPrefabLink") continue;

                newComps.Append(std::move(compObj));
            }
            goObj.Set("components", std::move(newComps));
        }
        goArr.Append(std::move(goObj));
    }

    JsonObject fileRoot;
    fileRoot.Set("name",        root.GetName());
    fileRoot.Set("version",     1.0);
    fileRoot.Set("GameObjects", std::move(goArr));
    JsonFile::SaveToFile(fileRoot, filePath);

    for (auto go : allGOs)
    {
        if (auto* cm = go.TryGetComponent<CMaterial>())
            if (cm->material && !cm->material->GetAssetsPath().empty())
                ImporterMaterial::SaveMaterialToAssets(cm->material);
    }

    NOUS_INFO_C(CURRENT_CHANNEL, "[PrefabManager] Saved prefab '%s' → %s (%zu object(s))",
        root.GetName().c_str(), filePath.c_str(), allGOs.size());
}

// -----------------------------------------------------------------------------
// InstantiatePrefab
// -----------------------------------------------------------------------------
GameObject PrefabManager::InstantiatePrefab(const std::string& filePath, Scene* scene, GameObject parent)
{
    if (!scene)
    {
        NOUS_ERROR("[PrefabManager] InstantiatePrefab called with null scene.");
        return {};
    }

    JsonObject fileRoot = JsonFile::LoadFromFile(filePath);
    if (fileRoot.IsEmpty())
    {
        NOUS_ERROR("[PrefabManager] Failed to parse prefab file: %s", filePath.c_str());
        return {};
    }

    const int version = static_cast<int>(fileRoot.GetDouble("version", 0.0));
    if (version != 1)
        NOUS_WARN_C(CURRENT_CHANNEL, "[PrefabManager] InstantiatePrefab: unexpected version %d in '%s' — proceeding anyway.", version, filePath.c_str());
    else
        NOUS_DEBUG("[PrefabManager] InstantiatePrefab: loading '%s' (version %d)", filePath.c_str(), version);

    JsonArray arr = fileRoot.GetArray("GameObjects");
    if (arr.IsEmpty())
    {
        NOUS_ERROR("[PrefabManager] No GameObjects array in prefab: %s", filePath.c_str());
        return {};
    }

    struct GOEntry
    {
        GameObject go;
        uint32_t   prefabParentID;
    };

    std::vector<GOEntry>                     entries;
    std::unordered_map<uint32_t, GameObject> prefabIDToGO;

    const int count = arr.Count();
    for (int i = 0; i < count; ++i)
    {
        JsonObject obj          = arr.GetObject(i);
        uint32_t   prefabUID    = static_cast<uint32_t>(obj.GetDouble("uid",    0.0));
        uint32_t   prefabParent = static_cast<uint32_t>(obj.GetDouble("parent", 0.0));
        std::string name        = obj.GetString("name");

        // CreateGameObjectDetached generates a fresh scene-unique ID — no collision risk.
        GameObject go = scene->CreateGameObjectDetached(name.empty() ? "GameObject" : name);

        // Deserialize components
        JsonArray comps = obj.GetArray("components");
        if (!comps.IsEmpty())
        {
            const int compCount = comps.Count();
            for (int j = 0; j < compCount; ++j)
            {
                JsonObject  compObj  = comps.GetObject(j);
                std::string typeName = compObj.GetString("type");
                if (typeName.empty()) continue;
                // Skip CPrefab during instantiation — set fresh on root below.
                if (typeName == "CPrefab") continue;
                DeserializeComponentInto(go, typeName, compObj);
            }
        }

        // Every object the prefab creates is prefab-OWNED. Anything the user adds
        // later has no link, which is how Update tells them apart.
        go.AddComponent<CPrefabLink>().prefabObjectID = prefabUID;

        prefabIDToGO[prefabUID] = go;
        entries.push_back({ go, prefabParent });
    }

    // Wire parent-child hierarchy using the prefab-UID → GO mapping.
    for (auto& entry : entries)
    {
        if (entry.prefabParentID == 0) continue;

        auto it = prefabIDToGO.find(entry.prefabParentID);
        if (it != prefabIDToGO.end())
            it->second.AddChild(entry.go);
        else
        {
            NOUS_WARN_C(CURRENT_CHANNEL, "[PrefabManager] Parent prefab UID %u not found — destroying orphaned GO '%s'.",
                entry.prefabParentID, entry.go.GetName().c_str());
            scene->DestroyGameObject(entry.go);
        }
    }

    // Find the root GO (no parent in prefab).
    GameObject prefabRoot;
    for (auto& entry : entries)
    {
        if (entry.prefabParentID == 0)
        {
            prefabRoot = entry.go;
            break;
        }
    }

    if (!prefabRoot.IsValid())
    {
        NOUS_ERROR("[PrefabManager] No root GO found in prefab: %s", filePath.c_str());
        for (auto& entry : entries)
            scene->DestroyGameObject(entry.go);
        return {};
    }

    if (parent.IsValid())
        parent.AddChild(prefabRoot);

    // Attach CPrefab to the root so the scene knows it's a prefab instance.
    auto& cprefab = prefabRoot.AddComponent<CPrefab>();
    cprefab.prefabSourcePath = filePath;
    cprefab.syncedHash       = HashPrefabFile(filePath);
    cprefab.isStale          = false;

    // Register all instantiated GOs into the scene.
    for (auto& entry : entries)
        scene->RegisterGameObject(entry.go);

    NOUS_INFO_C(CURRENT_CHANNEL, "[PrefabManager] Instantiated prefab '%s' → root GO '%s' (%zu object(s))",
        filePath.c_str(), prefabRoot.GetName().c_str(), entries.size());
    return prefabRoot;
}

// -----------------------------------------------------------------------------
// ReloadPrefabInstance
// -----------------------------------------------------------------------------
void PrefabManager::ReloadPrefabInstance(GameObject instanceRoot, Scene* scene)
{
    if (!instanceRoot.IsValid() || !scene) return;

    auto* cprefab = instanceRoot.TryGetComponent<CPrefab>();
    if (!cprefab)
    {
        NOUS_WARN_C(CURRENT_CHANNEL, "[PrefabManager] ReloadPrefabInstance called on GO without CPrefab.");
        return;
    }

    const std::string prefabPath = cprefab->prefabSourcePath;
    if (!std::filesystem::exists(prefabPath))
    {
        NOUS_WARN_C(CURRENT_CHANNEL, "[PrefabManager] Prefab source file missing: %s — skipping reload.", prefabPath.c_str());
        return;
    }

    // Destroy all current children (stale inline scene data).
    const auto children = instanceRoot.GetChildren();
    for (auto child : children)
        scene->DestroyGameObject(child);

    JsonObject fileRoot = JsonFile::LoadFromFile(prefabPath);
    if (fileRoot.IsEmpty())
    {
        NOUS_ERROR("[PrefabManager] ReloadPrefabInstance: failed to parse %s", prefabPath.c_str());
        return;
    }

    JsonArray arr = fileRoot.GetArray("GameObjects");
    if (arr.IsEmpty()) return;

    // Collect stale components on the root (those no longer in the prefab definition).
    {
        std::unordered_set<std::string> prefabRootTypes;
        const int preCount = arr.Count();
        for (int i = 0; i < preCount; ++i)
        {
            JsonObject obj = arr.GetObject(i);
            if (static_cast<uint32_t>(obj.GetDouble("parent", 0.0)) != 0) continue;

            JsonArray comps = obj.GetArray("components");
            if (!comps.IsEmpty())
            {
                for (int j = 0; j < comps.Count(); ++j)
                {
                    JsonObject  compObj  = comps.GetObject(j);
                    std::string typeName = compObj.GetString("type");
                    if (!typeName.empty()) prefabRootTypes.insert(typeName);
                }
            }
            break; // only one root entry
        }

        std::vector<std::string> toRemove;
        for (auto* comp : instanceRoot.GetAllComponents())
        {
            const std::string t(comp->GetType());
            // CPrefabLink is engine bookkeeping, never present in the asset — it is
            // re-stamped below, so removing it here would be churn.
            if (t == "CTransform" || t == "CPrefab" || t == "CPrefabLink" || t == "CScript") continue;
            if (prefabRootTypes.find(t) == prefabRootTypes.end())
                toRemove.push_back(t);
        }
        for (const auto& typeName : toRemove)
            RemoveComponentByName(instanceRoot, typeName);

        if (!toRemove.empty())
            NOUS_INFO_C(CURRENT_CHANNEL, "[PrefabManager] ReloadPrefabInstance: removed %zu stale component(s) from root '%s'.",
                toRemove.size(), instanceRoot.GetName().c_str());
    }

    struct GOEntry
    {
        GameObject go;
        uint32_t   prefabParentID;
        uint32_t   prefabUID;
    };

    std::vector<GOEntry>                     entries;
    std::unordered_map<uint32_t, GameObject> prefabIDToGO;

    const int count = arr.Count();
    for (int i = 0; i < count; ++i)
    {
        JsonObject  obj         = arr.GetObject(i);
        uint32_t    prefabUID   = static_cast<uint32_t>(obj.GetDouble("uid",    0.0));
        uint32_t    prefabParent = static_cast<uint32_t>(obj.GetDouble("parent", 0.0));
        std::string name        = obj.GetString("name");

        if (prefabParent == 0)
        {
            // This is the prefab root — map its UID to the existing instanceRoot.
            prefabIDToGO[prefabUID] = instanceRoot;

            if (!name.empty())
                instanceRoot.SetName(name);

            // Reload root's non-structural components.
            JsonArray comps = obj.GetArray("components");
            if (!comps.IsEmpty())
            {
                const int compCount = comps.Count();
                for (int j = 0; j < compCount; ++j)
                {
                    JsonObject  compObj  = comps.GetObject(j);
                    std::string typeName = compObj.GetString("type");
                    if (typeName.empty())      continue;
                    if (typeName == "CTransform") continue;
                    if (typeName == "CPrefab")    continue;
                    if (typeName == "CScript")    continue;
                    DeserializeComponentInto(instanceRoot, typeName, compObj);
                }
            }

            // The root is prefab-owned too. Assign rather than add, since a
            // re-linked instance may already carry one.
            if (auto* existing = instanceRoot.TryGetComponent<CPrefabLink>())
                existing->prefabObjectID = prefabUID;
            else
                instanceRoot.AddComponent<CPrefabLink>().prefabObjectID = prefabUID;
        }
        else
        {
            // Child GO — create fresh, reusing the prefab's UID so that repeated
            // reload/save cycles don't generate a new random UID each time.
            GameObject go = scene->CreateGameObjectDetached(name.empty() ? "GameObject" : name, nullptr, prefabUID);

            JsonArray comps = obj.GetArray("components");
            if (!comps.IsEmpty())
            {
                const int compCount = comps.Count();
                for (int j = 0; j < compCount; ++j)
                {
                    JsonObject  compObj  = comps.GetObject(j);
                    std::string typeName = compObj.GetString("type");
                    if (typeName.empty())      continue;
                    if (typeName == "CPrefab") continue;
                    DeserializeComponentInto(go, typeName, compObj);
                }
            }

            go.AddComponent<CPrefabLink>().prefabObjectID = prefabUID;

            prefabIDToGO[prefabUID] = go;
            entries.push_back({ go, prefabParent, prefabUID });
            scene->RegisterGameObject(go);
        }
    }

    // Wire parent-child for new GOs.
    for (auto& entry : entries)
    {
        auto it = prefabIDToGO.find(entry.prefabParentID);
        if (it != prefabIDToGO.end())
            it->second.AddChild(entry.go);
    }

    cprefab->syncedHash = HashPrefabFile(prefabPath);
    cprefab->isStale    = false;

    NOUS_INFO_C(CURRENT_CHANNEL, "[PrefabManager] Reloaded prefab instance '%s' from '%s' (%zu child(ren) rebuilt).",
        instanceRoot.GetName().c_str(), prefabPath.c_str(), entries.size());
}

// -----------------------------------------------------------------------------
// UpdateFromPrefab
// -----------------------------------------------------------------------------
void PrefabManager::UpdateFromPrefab(GameObject instanceRoot, Scene* scene)
{
    if (!instanceRoot.IsValid() || !scene) return;

    auto* cprefab = instanceRoot.TryGetComponent<CPrefab>();
    if (!cprefab)
    {
        NOUS_WARN_C(CURRENT_CHANNEL, "[PrefabManager] UpdateFromPrefab called on GO without CPrefab.");
        return;
    }

    const std::string prefabPath = cprefab->prefabSourcePath;
    if (!std::filesystem::exists(prefabPath))
    {
        NOUS_WARN_C(CURRENT_CHANNEL, "[PrefabManager] Prefab source file missing: %s — skipping update.", prefabPath.c_str());
        return;
    }

    std::vector<GameObject> subtree;
    CollectSubtree(instanceRoot, subtree);

    std::unordered_map<uint32_t, GameObject> linkToGO;
    for (GameObject& go : subtree)
        if (const auto* link = go.TryGetComponent<CPrefabLink>())
            linkToGO[link->prefabObjectID] = go;

    // MIGRATION: no links anywhere means this instance was saved before prefab
    // overrides existed. Rebuild it once -- that stamps links throughout -- and stop.
    // Merging without links would treat every prefab object as user-added and
    // duplicate the entire prefab on top of the instance.
    if (linkToGO.empty())
    {
        NOUS_INFO_C(CURRENT_CHANNEL, "[PrefabManager] '%s' has no prefab links (pre-override scene) — rebuilding once to relink.",
            instanceRoot.GetName().c_str());
        ReloadPrefabInstance(instanceRoot, scene);
        return;
    }

    JsonObject fileRoot = JsonFile::LoadFromFile(prefabPath);
    if (fileRoot.IsEmpty())
    {
        NOUS_ERROR("[PrefabManager] UpdateFromPrefab: failed to parse %s", prefabPath.c_str());
        return;
    }

    JsonArray arr = fileRoot.GetArray("GameObjects");
    if (arr.IsEmpty()) return;

    std::unordered_map<uint32_t, GameObject>     prefabIDToGO;
    std::unordered_set<uint32_t>                 assetIDs;
    std::vector<std::pair<GameObject, uint32_t>> newlyCreated;   // (go, prefabParentID)

    // ---- Pass 1: refresh linked objects, create missing ones --------------------
    const int count = arr.Count();
    for (int i = 0; i < count; ++i)
    {
        JsonObject        obj          = arr.GetObject(i);
        const uint32_t    prefabUID    = static_cast<uint32_t>(obj.GetDouble("uid",    0.0));
        const uint32_t    prefabParent = static_cast<uint32_t>(obj.GetDouble("parent", 0.0));
        const std::string name         = obj.GetString("name");
        const bool        isRoot       = (prefabParent == 0);

        assetIDs.insert(prefabUID);

        GameObject go;
        if (isRoot)
        {
            // The root is always THIS instance's root, never created. Looking it up
            // by link would create a second root if the link were ever lost.
            go = instanceRoot;
            if (auto* link = go.TryGetComponent<CPrefabLink>())
                link->prefabObjectID = prefabUID;
            else
                go.AddComponent<CPrefabLink>().prefabObjectID = prefabUID;
        }
        else if (const auto it = linkToGO.find(prefabUID); it != linkToGO.end())
        {
            go = it->second;
        }
        else
        {
            go = scene->CreateGameObjectDetached(name.empty() ? "GameObject" : name, nullptr, prefabUID);
            go.AddComponent<CPrefabLink>().prefabObjectID = prefabUID;
            scene->RegisterGameObject(go);
            newlyCreated.emplace_back(go, prefabParent);
        }

        if (!name.empty()) go.SetName(name);

        JsonArray comps = obj.GetArray("components");
        if (!comps.IsEmpty())
        {
            const int compCount = comps.Count();
            for (int j = 0; j < compCount; ++j)
            {
                JsonObject        compObj  = comps.GetObject(j);
                const std::string typeName = compObj.GetString("type");
                if (typeName.empty())          continue;
                if (typeName == "CPrefab")     continue;
                if (typeName == "CPrefabLink") continue;

                // Instance placement is per-instance. Overwriting it would teleport
                // every instance to the prefab's authored position.
                if (isRoot && typeName == "CTransform") continue;

                DeserializeComponentInto(go, typeName, compObj);
            }
        }

        // NOTE: components the asset does NOT declare are deliberately left in place.
        // A component carries no link, so "the prefab deleted it" and "the user added
        // it" are indistinguishable -- and removing them would strip a
        // CBoneAttachment off an instance root, which is the bug this feature fixes.

        prefabIDToGO[prefabUID] = go;
    }

    // ---- Pass 2: parent newly created objects -----------------------------------
    for (auto& [go, prefabParentID] : newlyCreated)
    {
        if (const auto it = prefabIDToGO.find(prefabParentID); it != prefabIDToGO.end())
            it->second.AddChild(go);
        else
            instanceRoot.AddChild(go);   // asset hierarchy is broken; do not orphan it
    }

    // ---- Pass 3: destroy linked objects the asset dropped -----------------------
    for (GameObject& go : subtree)
    {
        if (go == instanceRoot) continue;
        if (!go.IsValid())      continue;   // already destroyed with an ancestor

        const auto* link = go.TryGetComponent<CPrefabLink>();
        if (!link)                                    continue;  // user-added: untouchable
        if (assetIDs.contains(link->prefabObjectID))  continue;  // still in the asset

        // Rescue user-added descendants before their prefab-owned parent goes.
        for (GameObject child : go.GetChildren())
            if (child.IsValid() && !child.HasComponent<CPrefabLink>())
                instanceRoot.AddChild(child);

        scene->DestroyGameObject(go);
    }

    cprefab->syncedHash = HashPrefabFile(prefabPath);
    cprefab->isStale    = false;

    NOUS_INFO_C(CURRENT_CHANNEL, "[PrefabManager] Updated instance '%s' from '%s'.",
        instanceRoot.GetName().c_str(), prefabPath.c_str());
}

// -----------------------------------------------------------------------------
// ApplyToPrefab
// -----------------------------------------------------------------------------
void PrefabManager::ApplyToPrefab(GameObject instanceRoot, Scene* scene)
{
    if (!instanceRoot.IsValid()) return;

    auto* cprefab = instanceRoot.TryGetComponent<CPrefab>();
    if (!cprefab || cprefab->prefabSourcePath.empty())
    {
        NOUS_WARN_C(CURRENT_CHANNEL, "[PrefabManager] ApplyToPrefab called on a GO that is not a prefab instance.");
        return;
    }

    const std::string prefabPath = cprefab->prefabSourcePath;

    // Everything in the subtree is about to become part of the prefab, so everything
    // needs an identity in it. SavePrefab writes each object's scene UID as "uid",
    // so that is the id the saved file will carry.
    std::vector<GameObject> subtree;
    CollectSubtree(instanceRoot, subtree);
    for (GameObject& go : subtree)
        if (!go.HasComponent<CPrefabLink>())
            go.AddComponent<CPrefabLink>().prefabObjectID = go.GetID();

    SavePrefab(instanceRoot, prefabPath);

    cprefab->syncedHash = HashPrefabFile(prefabPath);
    cprefab->isStale    = false;

    // Every other instance of this prefab is now behind the asset. Nothing else would
    // tell them: UpdatePrefabStaleFlags runs only on scene load.
    if (scene)
    {
        auto& registry = scene->GetRegistry();
        for (auto entity : registry.view<CPrefab>())
        {
            if (entity == instanceRoot.GetEntity()) continue;

            CPrefab& other = registry.get<CPrefab>(entity);
            if (other.prefabSourcePath == prefabPath)
                other.isStale = true;
        }
    }

    NOUS_INFO_C(CURRENT_CHANNEL, "[PrefabManager] Applied instance '%s' to '%s'.",
        instanceRoot.GetName().c_str(), prefabPath.c_str());
}
