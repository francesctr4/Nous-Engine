#ifndef NOUS_ENGINE_SCENE_H
#define NOUS_ENGINE_SCENE_H

#include "Engine/EngineExport.h"
#include "Engine/Utils/DataStructures/NOUS_Vector.h"

#include <memory>
#include <mutex>

class GameObject;

class Scene {
public:
    explicit Scene(const std::string& name = "Untitled Scene");
    ~Scene();

    NOUS_ENGINE_API GameObject* CreateGameObject(const std::string& name = "GameObject", GameObject* parent = nullptr);
    NOUS_ENGINE_API void DestroyGameObject(GameObject* go);
    void Update(float deltaTime);

    GameObject* FindGameObjectByID(uint32_t id);
    NOUS_Vector<GameObject*> FindGameObjectsByName(const std::string& name);
    GameObject* GetGameObjectByID(uint32_t id);

    uint32_t CreateGameObjectID(const std::string& name = "GameObject", GameObject* parent = nullptr);
    void DestroyGameObjectByID(uint32_t id);

    NOUS_ENGINE_API const std::string& GetName() const;
    void SetName(const std::string& name);

    NOUS_ENGINE_API NOUS_Vector<GameObject*>& GetGameObjects();

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
};

#endif // NOUS_ENGINE_SCENE_H