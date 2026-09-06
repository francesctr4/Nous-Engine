#pragma once

#include <ECS/Component/Component.h>
#include <EngineCore/EngineExport.h>

#include <cstdint>

/**
 * @brief Marks a GameObject as OWNED by the prefab its instance root points at.
 *
 * Present on every object a prefab instantiated, including the instance root. Its
 * ABSENCE is the signal that matters: an object inside an instance with no
 * CPrefabLink was added by the user, and no prefab operation may touch it.
 *
 * This exists rather than matching on the scene UID because
 * Scene::CreateGameObjectDetached honours a preferred UID only when it is free --
 * so the SECOND instance of a prefab in one scene gets fresh random UIDs for its
 * children, and UID matching silently fails for it. (Unity solves this the same
 * way, with m_CorrespondingSourceObject.)
 *
 * Engine bookkeeping, not user-facing: it is never offered in the Add Component
 * menu, because a hand-added one would claim prefab ownership of an object the
 * prefab has never heard of.
 */
class CPrefabLink : public Component {
public:
    COMPONENT_TYPE(CPrefabLink)

    // This object's "uid" in the .nprefab file. 0 means unset.
    uint32_t prefabObjectID = 0;

    NOUS_ENGINE_API JsonObject Serialize()              const override;
    NOUS_ENGINE_API void       Deserialize(const JsonObject& obj) override;
};
