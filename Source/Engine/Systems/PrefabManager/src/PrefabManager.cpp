#include "Engine/Systems/PrefabManager/include/PrefabManager.h"

#include "Engine/Systems/ECS/Scene/include/Scene.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/ECS/Component/Component.h"
#include "Engine/Systems/ECS/Component/Types/CPrefab/include/CPrefab.h"
#include "Engine/Systems/ECS/Component/Types/CTransform/include/CTransform.h"
#include "Engine/Systems/ECS/Component/Types/CMesh/include/CMesh.h"
#include "Engine/Systems/ECS/Component/Types/CMaterial/include/CMaterial.h"
#include "Engine/Systems/ResourceManager/Types/ResourceMaterial/include/ResourceMaterial.h"
#include "Engine/Systems/ResourceManager/Types/ResourceMaterial/include/ImporterMaterial.h"
#include "Engine/Systems/ECS/Component/Types/CCamera/include/CCamera.h"
#include "Engine/Systems/ECS/Component/Types/CLight/include/CLight.h"
#include "Engine/Systems/ECS/Component/Types/CScript/include/CScript.h"
#include "Engine/Systems/ECS/Component/Types/ComponentTypes.h"
#include "Engine/Core/Logger/Logger.h"

#include "Engine/Utils/Serialization/JsonFile/JsonFile.h"
#include "Engine/Utils/Serialization/JsonFile/JsonArray.h"
#include <filesystem>
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
                if (compObj.GetString("type") == "CPrefab") continue;
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
            if (t == "CTransform" || t == "CPrefab" || t == "CScript") continue;
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

    NOUS_INFO_C(CURRENT_CHANNEL, "[PrefabManager] Reloaded prefab instance '%s' from '%s' (%zu child(ren) rebuilt).",
        instanceRoot.GetName().c_str(), prefabPath.c_str(), entries.size());
}
