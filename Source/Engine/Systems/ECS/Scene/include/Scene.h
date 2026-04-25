#pragma once

#include "Engine/EngineExport.h"
#include "Engine/Systems/ECS/ECSInternalComponents.h"

#include <entt/entt.hpp>
#include <mutex>
#include <string>
#include <unordered_map>
#include <cstdint>
#include <vector>

class GameObject;
class ModuleScene;
class ModuleResourceManager;

class Scene {
public:
    NOUS_ENGINE_API explicit Scene(const std::string& name = "Untitled Scene",
                                   ModuleScene* moduleScene = nullptr,
                                   ModuleResourceManager* resourceManager = nullptr);
    NOUS_ENGINE_API ~Scene();

    // Registry access.
    NOUS_ENGINE_API       entt::registry& GetRegistry()       { return m_Registry; }
    NOUS_ENGINE_API const entt::registry& GetRegistry() const { return m_Registry; }

    ModuleScene*           GetModuleScene()     const { return m_ModuleScene; }
    ModuleResourceManager* GetResourceManager() const { return m_ResourceManager; }

    // GameObject creation / destruction
    NOUS_ENGINE_API GameObject CreateGameObject(const std::string& name = "GameObject",
                                                GameObject* parent = nullptr);
    NOUS_ENGINE_API void       DestroyGameObject(GameObject go);

    // Thread-safe deferred registration (for worker-thread scene loading)
    NOUS_ENGINE_API GameObject CreateGameObjectDetached(const std::string& name = "GameObject",
                                                        GameObject* parent = nullptr,
                                                        uint32_t preferredUID = 0);
    NOUS_ENGINE_API void       RegisterGameObject(GameObject go);

    // Update
    void Update(float deltaTime);
    NOUS_ENGINE_API void UpdateWorldMatrices();

    // Lookup
    NOUS_ENGINE_API GameObject FindGameObjectByID(uint32_t id);
    NOUS_ENGINE_API GameObject GetGameObjectByID(uint32_t id);
    NOUS_ENGINE_API GameObject FindGameObjectByName(const std::string& name);

    // Returns all GameObjects as a vector of handles.
    // For render hot paths, prefer GetRegistry().view<>() directly.
    NOUS_ENGINE_API std::vector<GameObject> GetGameObjects() const;

    // Thread-safe snapshot (for editor iteration while scene loads in background)
    NOUS_ENGINE_API std::vector<GameObject> GetGameObjectsSnapshot() const;

    NOUS_ENGINE_API const std::string& GetName() const;
    void SetName(const std::string& name);

    void Serialize(const std::string& filepath) const;
    void Deserialize(const std::string& filepath);
    NOUS_ENGINE_API void Clear();

private:
    void         CollectEntityTree(entt::entity root, std::vector<entt::entity>& out);
    void         DestroyEntity(entt::entity entity);
    entt::entity FindEntityByID_NoLock(uint32_t id) const;
    uint32_t     GenerateUniqueID();

    entt::registry                             m_Registry;
    std::unordered_map<uint32_t, entt::entity> m_IDToEntity;
    std::vector<entt::entity>                  m_OrderedEntities;
    std::string                                m_Name;
    mutable std::mutex                         m_Mutex;
    ModuleScene*                               m_ModuleScene     = nullptr;
    ModuleResourceManager*                     m_ResourceManager = nullptr;
};
