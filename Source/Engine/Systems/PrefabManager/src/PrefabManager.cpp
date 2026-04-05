#include "Engine/Systems/PrefabManager/include/PrefabManager.h"

#include "Engine/Systems/ECS/Scene/include/Scene.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/ECS/Component/Component.h"
#include "Engine/Systems/ECS/Component/CPrefab/include/CPrefab.h"
#include "Engine/Core/Logger/Logger.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"

#include <parson.h>
#include <filesystem>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstring>

constexpr LogChannel CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_CORE_MODULE_SCENE;

// -----------------------------------------------------------------------------
// SavePrefab
// -----------------------------------------------------------------------------
void PrefabManager::SavePrefab(GameObject* root, const std::string& filePath)
{
    if (!root)
    {
        NOUS_ERROR("[PrefabManager] SavePrefab called with null root.");
        return;
    }

    std::filesystem::create_directories(std::filesystem::path(filePath).parent_path());

    // BFS to collect root + all descendants
    std::vector<GameObject*> allGOs;
    std::queue<GameObject*> bfsQueue;
    bfsQueue.push(root);
    while (!bfsQueue.empty())
    {
        GameObject* current = bfsQueue.front();
        bfsQueue.pop();
        allGOs.push_back(current);
        for (auto* child : current->GetChildren())
            bfsQueue.push(child);
    }

    JSON_Value* fileRoot = json_value_init_object();
    JSON_Object* fileObj  = json_value_get_object(fileRoot);
    json_object_set_string(fileObj, "name", root->GetName().c_str());
    json_object_set_number(fileObj, "version", 1);

    JSON_Value* arrVal = json_value_init_array();
    JSON_Array* arr    = json_value_get_array(arrVal);

    for (auto* go : allGOs)
    {
        JSON_Value* goVal    = go->Serialize();
        JSON_Object* goObj   = json_value_get_object(goVal);

        // The prefab root must always have parent=0 in the file regardless of
        // its position in the scene hierarchy, so ReloadPrefabInstance can
        // reliably identify it.
        if (go == root)
            json_object_set_number(goObj, "parent", 0);

        // Strip CPrefab from the saved data — the file IS the prefab definition,
        // so embedding a self-referential CPrefab component is meaningless and
        // would confuse ReloadPrefabInstance when it parses the file.
        JSON_Array* comps = json_object_get_array(goObj, "components");
        if (comps)
        {
            for (size_t ci = json_array_get_count(comps); ci-- > 0;)
            {
                JSON_Object* compObj  = json_array_get_object(comps, ci);
                const char*  typeName = json_object_get_string(compObj, "type");
                if (typeName && std::string(typeName) == "CPrefab")
                    json_array_remove(comps, ci);
            }
        }
        json_array_append_value(arr, goVal);
    }

    json_object_set_value(fileObj, "GameObjects", arrVal);
    json_serialize_to_file_pretty(fileRoot, filePath.c_str());
    json_value_free(fileRoot);

    NOUS_INFO_C(CURRENT_CHANNEL, "[PrefabManager] Saved prefab '%s' → %s (%zu object(s))",
        root->GetName().c_str(), filePath.c_str(), allGOs.size());
}

// -----------------------------------------------------------------------------
// InstantiatePrefab
// -----------------------------------------------------------------------------
GameObject* PrefabManager::InstantiatePrefab(const std::string& filePath, Scene* scene, GameObject* parentGO)
{
    if (!scene)
    {
        NOUS_ERROR("[PrefabManager] InstantiatePrefab called with null scene.");
        return nullptr;
    }

    JSON_Value* fileRoot = json_parse_file(filePath.c_str());
    if (!fileRoot)
    {
        NOUS_ERROR("[PrefabManager] Failed to parse prefab file: %s", filePath.c_str());
        return nullptr;
    }

    JSON_Object* fileObj = json_value_get_object(fileRoot);

    const int version = static_cast<int>(json_object_get_number(fileObj, "version"));
    if (version != 1)
        NOUS_WARN_C(CURRENT_CHANNEL, "[PrefabManager] InstantiatePrefab: unexpected version %d in '%s' — proceeding anyway.", version, filePath.c_str());
    else
        NOUS_DEBUG("[PrefabManager] InstantiatePrefab: loading '%s' (version %d)", filePath.c_str(), version);

    JSON_Array*  arr     = json_object_get_array(fileObj, "GameObjects");
    if (!arr)
    {
        NOUS_ERROR("[PrefabManager] No GameObjects array in prefab: %s", filePath.c_str());
        json_value_free(fileRoot);
        return nullptr;
    }

    struct GOEntry
    {
        GameObject* go;
        uint32_t    prefabParentID; // original parent UID from the prefab file (0 = root)
    };

    std::vector<GOEntry>                       entries;
    std::unordered_map<uint32_t, GameObject*>  prefabIDToGO;

    const size_t count = json_array_get_count(arr);
    for (size_t i = 0; i < count; ++i)
    {
        JSON_Object* obj           = json_array_get_object(arr, i);
        uint32_t     prefabUID     = static_cast<uint32_t>(json_object_get_number(obj, "uid"));
        uint32_t     prefabParent  = static_cast<uint32_t>(json_object_get_number(obj, "parent"));
        const char*  name          = json_object_get_string(obj, "name");

        // CreateGameObjectDetached generates a fresh scene-unique ID — no collision risk.
        GameObject* go = scene->CreateGameObjectDetached(name ? name : "GameObject");

        // Deserialize components
        JSON_Array* comps = json_object_get_array(obj, "components");
        if (comps)
        {
            const size_t compCount = json_array_get_count(comps);
            for (size_t j = 0; j < compCount; ++j)
            {
                JSON_Object* compObj  = json_array_get_object(comps, j);
                const char*  typeName = json_object_get_string(compObj, "type");
                if (!typeName) continue;

                // Skip CPrefab during instantiation — we set it fresh on the root below.
                if (std::string(typeName) == "CPrefab") continue;

                Component* comp = Component::CreateComponent(typeName);
                if (comp)
                {
                    comp->Deserialize(compObj);
                    go->AddComponent(comp);
                }
            }
        }

        prefabIDToGO[prefabUID] = go;
        entries.push_back({ go, prefabParent });
    }

    json_value_free(fileRoot);

    // Wire parent-child hierarchy using the prefab-UID → GO mapping.
    for (auto& entry : entries)
    {
        if (entry.prefabParentID == 0) continue;

        auto it = prefabIDToGO.find(entry.prefabParentID);
        if (it != prefabIDToGO.end())
            it->second->AddChild(entry.go);
        else
        {
            NOUS_WARN_C(CURRENT_CHANNEL, "[PrefabManager] Parent prefab UID %u not found — destroying orphaned GO '%s'.",
                entry.prefabParentID, entry.go->GetName().c_str());
            scene->DestroyGameObject(entry.go);
        }
    }

    // Find the root GO (no parent in prefab) and optionally parent it to parentGO.
    GameObject* prefabRoot = nullptr;
    for (auto& entry : entries)
    {
        if (entry.prefabParentID == 0)
        {
            prefabRoot = entry.go;
            break;
        }
    }

    if (!prefabRoot)
    {
        NOUS_ERROR("[PrefabManager] No root GO found in prefab: %s", filePath.c_str());
        for (auto& entry : entries)
            NOUS_DELETE(entry.go, MemoryTag::GAMEOBJECT);
        return nullptr;
    }

    if (parentGO)
        parentGO->AddChild(prefabRoot);

    // Attach CPrefab to the root so the scene knows it's a prefab instance.
    auto& cprefab = prefabRoot->AddComponent<CPrefab>();
    cprefab.prefabSourcePath = filePath;

    // Register all instantiated GOs into the scene.
    for (auto& entry : entries)
        scene->RegisterGameObject(entry.go);

    NOUS_INFO_C(CURRENT_CHANNEL, "[PrefabManager] Instantiated prefab '%s' → root GO '%s' (%zu object(s))",
        filePath.c_str(), prefabRoot->GetName().c_str(), entries.size());
    return prefabRoot;
}

// -----------------------------------------------------------------------------
// ReloadPrefabInstance
// -----------------------------------------------------------------------------
void PrefabManager::ReloadPrefabInstance(GameObject* instanceRoot, Scene* scene)
{
    if (!instanceRoot || !scene) return;

    auto* cprefab = instanceRoot->TryGetComponent<CPrefab>();
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
    // Copy to avoid iterator invalidation during destruction.
    const auto children = instanceRoot->GetChildren();
    for (auto* child : children)
        scene->DestroyGameObject(child);

    // Parse the prefab file and instantiate only the non-root GOs as children.
    JSON_Value* fileRoot = json_parse_file(prefabPath.c_str());
    if (!fileRoot)
    {
        NOUS_ERROR("[PrefabManager] ReloadPrefabInstance: failed to parse %s", prefabPath.c_str());
        return;
    }

    JSON_Object* fileObj = json_value_get_object(fileRoot);
    JSON_Array*  arr     = json_object_get_array(fileObj, "GameObjects");
    if (!arr) { json_value_free(fileRoot); return; }

    // Before rebuilding, collect which component types the prefab root defines.
    // Any component currently on instanceRoot that is NOT CTransform, CPrefab, or in
    // that set is stale (removed from the prefab since last save) and must be stripped.
    {
        std::unordered_set<std::string> prefabRootTypes;
        const size_t preCount = json_array_get_count(arr);
        for (size_t i = 0; i < preCount; ++i)
        {
            JSON_Object* obj = json_array_get_object(arr, i);
            if (static_cast<uint32_t>(json_object_get_number(obj, "parent")) != 0) continue;

            JSON_Array* comps = json_object_get_array(obj, "components");
            if (comps)
            {
                for (size_t j = 0; j < json_array_get_count(comps); ++j)
                {
                    JSON_Object* compObj  = json_array_get_object(comps, j);
                    const char*  typeName = json_object_get_string(compObj, "type");
                    if (typeName) prefabRootTypes.insert(typeName);
                }
            }
            break; // only one root entry
        }

        // Collect stale type_indices before removing (avoid mutating while iterating).
        std::vector<std::type_index> toRemove;
        for (auto* comp : instanceRoot->GetAllComponents())
        {
            const std::string t = comp->GetType();
            if (t == "CTransform" || t == "CPrefab") continue;
            if (prefabRootTypes.find(t) == prefabRootTypes.end())
                toRemove.push_back(typeid(*comp));
        }
        for (const auto& ti : toRemove)
            instanceRoot->RemoveComponent(ti);

        if (!toRemove.empty())
            NOUS_INFO_C(CURRENT_CHANNEL, "[PrefabManager] ReloadPrefabInstance: removed %zu stale component(s) from root '%s'.",
                toRemove.size(), instanceRoot->GetName().c_str());
    }

    struct GOEntry
    {
        GameObject* go;
        uint32_t    prefabParentID;
        uint32_t    prefabUID;
    };

    std::vector<GOEntry>                      entries;
    std::unordered_map<uint32_t, GameObject*> prefabIDToGO;
    uint32_t                                   rootPrefabUID = 0;

    const size_t count = json_array_get_count(arr);
    for (size_t i = 0; i < count; ++i)
    {
        JSON_Object* obj          = json_array_get_object(arr, i);
        uint32_t     prefabUID    = static_cast<uint32_t>(json_object_get_number(obj, "uid"));
        uint32_t     prefabParent = static_cast<uint32_t>(json_object_get_number(obj, "parent"));
        const char*  name         = json_object_get_string(obj, "name");

        if (prefabParent == 0)
        {
            // This is the prefab root — map its UID to the existing instanceRoot.
            rootPrefabUID           = prefabUID;
            prefabIDToGO[prefabUID] = instanceRoot;

            // Update the root GO's name to match the prefab's root name.
            if (name && strlen(name) > 0)
                instanceRoot->SetName(name);
            // Also reload root's non-transform components (e.g. CMesh on prefab root).
            // We do this via the normal component deserialize path: skip CTransform and CPrefab.
            JSON_Array* comps = json_object_get_array(obj, "components");
            if (comps)
            {
                const size_t compCount = json_array_get_count(comps);
                for (size_t j = 0; j < compCount; ++j)
                {
                    JSON_Object* compObj  = json_array_get_object(comps, j);
                    const char*  typeName = json_object_get_string(compObj, "type");
                    if (!typeName) continue;
                    // Preserve the existing transform and prefab reference.
                    if (std::string(typeName) == "CTransform") continue;
                    if (std::string(typeName) == "CPrefab")    continue;

                    Component* comp = Component::CreateComponent(typeName);
                    if (comp)
                    {
                        instanceRoot->AddComponent(comp); // replaces if already present; also sets comp->m_GameObject
                        comp->Deserialize(compObj);
                    }
                }
            }
        }
        else
        {
            // Child GO — create fresh.
            GameObject* go = scene->CreateGameObjectDetached(name ? name : "GameObject");

            JSON_Array* comps = json_object_get_array(obj, "components");
            if (comps)
            {
                const size_t compCount = json_array_get_count(comps);
                for (size_t j = 0; j < compCount; ++j)
                {
                    JSON_Object* compObj  = json_array_get_object(comps, j);
                    const char*  typeName = json_object_get_string(compObj, "type");
                    if (!typeName) continue;
                    if (std::string(typeName) == "CPrefab") continue;

                    Component* comp = Component::CreateComponent(typeName);
                    if (comp)
                    {
                        go->AddComponent(comp); // sets comp->m_GameObject before Deserialize needs it
                        comp->Deserialize(compObj);
                    }
                }
            }

            prefabIDToGO[prefabUID] = go;
            entries.push_back({ go, prefabParent, prefabUID });
            scene->RegisterGameObject(go);
        }
    }

    json_value_free(fileRoot);

    // Wire parent-child for new GOs.
    for (auto& entry : entries)
    {
        auto it = prefabIDToGO.find(entry.prefabParentID);
        if (it != prefabIDToGO.end())
            it->second->AddChild(entry.go);
    }

    NOUS_INFO_C(CURRENT_CHANNEL, "[PrefabManager] Reloaded prefab instance '%s' from '%s' (%zu child(ren) rebuilt).",
        instanceRoot->GetName().c_str(), prefabPath.c_str(), entries.size());
}
