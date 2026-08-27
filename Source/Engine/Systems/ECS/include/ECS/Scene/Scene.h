#pragma once

#include "Engine/EngineExport.h"
#include <ECS/ECSInternalComponents.h>

#include <entt/entt.hpp>
#include <mutex>
#include <string>
#include <unordered_map>
#include <cstdint>
#include <vector>

#include <ECS/GameObject.h>
#include <ECS/ComponentServices.h>

class Scene {
public:
    NOUS_ENGINE_API explicit Scene(const std::string& name = "Untitled Scene",
                                   const ComponentServices* services = nullptr);
    NOUS_ENGINE_API ~Scene();

    // The engine services published to this scene's components. Never null:
    // returns an all-null aggregate when nothing was injected (headless / tests).
    NOUS_ENGINE_API const ComponentServices& GetServices() const;

    // Registry access.
    NOUS_ENGINE_API       entt::registry& GetRegistry()       { return m_registry; }
    NOUS_ENGINE_API const entt::registry& GetRegistry() const { return m_registry; }

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
    // Alias for FindGameObjectByID — kept for script-binding call-site readability ("get by id").
    GameObject GetGameObjectByID(uint32_t id) { return FindGameObjectByID(id); }
    NOUS_ENGINE_API GameObject FindGameObjectByName(const std::string& name);

    // Returns all GameObjects as a vector of handles.
    // For render hot paths, prefer GetRegistry().view<>() directly.
    NOUS_ENGINE_API std::vector<GameObject> GetGameObjects() const;

    // Alias for GetGameObjects — the name documents intent: the returned vector is a
    // copy, so iterating it is unaffected by structural changes made while iterating.
    std::vector<GameObject> GetGameObjectsSnapshot() const { return GetGameObjects(); }

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

    entt::registry                             m_registry;
    std::unordered_map<uint32_t, entt::entity> m_idToEntity;
    std::vector<entt::entity>                  m_orderedEntities;
    std::string                                m_name;
    // Guards m_idToEntity / m_orderedEntities / ID generation. As of the main-thread
    // ECS invariant every caller of those is on the main thread, so this is currently
    // uncontended. Kept deliberately: it costs nothing measurable and it is the only
    // thing standing between a future worker-thread scene lookup and a silent race
    // that only a ThreadSanitizer run on Linux would ever catch.
    mutable std::mutex                         m_mutex;
    // Borrowed, owned by ModuleScene. Outlives every Scene it is handed to.
    // This replaced the m_moduleScene / m_resourceManager back-pointers: Scene no
    // longer names anything in Modules/, and components reach engine services
    // through interfaces owned by Systems/ (see ComponentServices.h).
    const ComponentServices*                   m_services        = nullptr;
};
