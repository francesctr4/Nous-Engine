#ifndef NOUS_ENGINE_PREFABMANAGER_H
#define NOUS_ENGINE_PREFABMANAGER_H

#include "Engine/EngineExport.h"

#include <string>

class GameObject;
class Scene;

class PrefabManager
{
public:
    // Serializes root and its entire child hierarchy to filePath (.nprefab).
    // Creates intermediate directories if needed.
    // The CPrefab component on root (if any) is not written — the file IS the prefab definition.
    NOUS_ENGINE_API static void SavePrefab(GameObject* root, const std::string& filePath);

    // Deserializes a .nprefab file and instantiates the GO hierarchy into scene,
    // optionally parented to parentGO. A CPrefab component is attached to the
    // instantiated root pointing back to filePath.
    // Returns the instantiated root GO, or nullptr on failure.
    NOUS_ENGINE_API static GameObject* InstantiatePrefab(const std::string& filePath, Scene* scene, GameObject* parentGO = nullptr);

    // Refreshes an existing prefab instance rooted at instanceRoot:
    // destroys all current children and re-instantiates them from the source .nprefab file.
    // The instanceRoot GO itself (and its CTransform) is preserved.
    NOUS_ENGINE_API static void ReloadPrefabInstance(GameObject* instanceRoot, Scene* scene);
};

#endif // NOUS_ENGINE_PREFABMANAGER_H
