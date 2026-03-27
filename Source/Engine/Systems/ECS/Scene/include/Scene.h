#ifndef NOUS_ENGINE_SCENE_H
#define NOUS_ENGINE_SCENE_H

#include "Engine/EngineExport.h"
#include "Engine/Utils/DataStructures/NOUS_Vector.h"

#include <memory>
#include <mutex>
#include <string>

class GameObject;
class ModuleScene;
class ModuleResourceManager;

class Scene {
public:
    NOUS_ENGINE_API explicit Scene(const std::string& name = "Untitled Scene",
                                   ModuleScene* moduleScene = nullptr,
                                   ModuleResourceManager* resourceManager = nullptr);

    ModuleScene*           GetModuleScene()     const { return m_ModuleScene; }
    ModuleResourceManager* GetResourceManager() const { return m_ResourceManager; }
    NOUS_ENGINE_API ~Scene();

    NOUS_ENGINE_API GameObject* CreateGameObject(const std::string& name = "GameObject", GameObject* parent = nullptr);
    NOUS_ENGINE_API void DestroyGameObject(GameObject* go);

    // Thread-safe alternative for worker threads: create a GO without adding it to the scene,
    // set up all components, then call RegisterGameObject to make it visible to the main thread.
    NOUS_ENGINE_API GameObject* CreateGameObjectDetached(const std::string& name = "GameObject", GameObject* parent = nullptr);
    NOUS_ENGINE_API void RegisterGameObject(GameObject* go);
    void Update(float deltaTime);

    // Recomputes worldMatrix for every CTransform in the scene in top-down
    // (parent-before-child) order so child transforms inherit their parent's
    // world matrix correctly.  Call once per frame before rendering.
    void UpdateWorldMatrices();

    NOUS_ENGINE_API GameObject* FindGameObjectByID(uint32_t id);
    NOUS_Vector<GameObject*> FindGameObjectsByName(const std::string& name);
    NOUS_ENGINE_API GameObject* GetGameObjectByID(uint32_t id);

    uint32_t CreateGameObjectID(const std::string& name = "GameObject", GameObject* parent = nullptr);
    void DestroyGameObjectByID(uint32_t id);

    NOUS_ENGINE_API const std::string& GetName() const;
    void SetName(const std::string& name);

    NOUS_ENGINE_API NOUS_Vector<GameObject*>& GetGameObjects();

    // Returns a thread-safe snapshot copy of the game objects list.
    // Safe to call from the main thread concurrently with CreateGameObject() on a background thread.
    NOUS_ENGINE_API NOUS_Vector<GameObject*> GetGameObjectsSnapshot() const;

    void Serialize(const std::string& filepath) const;
    void Deserialize(const std::string& filepath);
    void Clear();

private:
    void CollectGameObjectTree(GameObject* root, NOUS_Vector<GameObject*>& collection);
    void DestroySingleGameObject(GameObject* go);
    GameObject* FindGameObjectByID_NoLock(uint32_t id);
    uint32_t GenerateUniqueID();

private:
    std::string m_Name;
    NOUS_Vector<GameObject*> m_GameObjects;
    mutable std::mutex m_Mutex;

    ModuleScene*           m_ModuleScene     = nullptr;
    ModuleResourceManager* m_ResourceManager = nullptr;
};

#endif // NOUS_ENGINE_SCENE_H