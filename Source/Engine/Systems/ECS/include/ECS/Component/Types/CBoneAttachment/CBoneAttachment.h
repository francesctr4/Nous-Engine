#pragma once

#include <ECS/Component/Component.h>
#include <EngineCore/EngineExport.h>

#include <glm/glm.hpp>

#include <string>

/**
 * @brief Hangs this GameObject off a bone of the nearest ancestor's CAnimator.
 *
 * The prop's OWN CTransform is the offset -- it simply becomes relative to the
 * bone instead of to the parent object. So the Inspector's Transform panel and
 * the Scene View gizmo are the editing surface, with no second transform to keep
 * in sync and nothing extra to serialize.
 *
 * Bones are not GameObjects (see CAnimator) and this component is what makes that
 * affordable: a 66-joint Mixamo rig would otherwise need 66 entities per character
 * just so a hat could be parented to one of them.
 *
 * EVERY FAILURE DEGRADES TO IDENTITY, meaning the object behaves as an ordinary
 * child: no ancestor holds a CAnimator, the animator is unbound, its pose has not
 * been sampled yet, boneName is empty, or the rig has no bone by that name.
 */
class CBoneAttachment : public Component {
public:
    COMPONENT_TYPE(CBoneAttachment)

    // "" leaves the object an ordinary child of its parent.
    std::string boneName;

    // Warn-once, mirroring CAnimator::warnedSkeletonMismatch. Mutable because
    // resolution reads this component through a const registry. Cleared whenever
    // boneName changes, so the next mistake gets its own warning.
    mutable bool warnedUnresolved = false;

    NOUS_ENGINE_API JsonObject Serialize()              const override;
    NOUS_ENGINE_API void       Deserialize(const JsonObject& obj) override;
};
