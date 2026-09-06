#include <ECS/Component/Types/CBoneAttachment/CBoneAttachment.h>

#include <ECS/Component/Types/CAnimator/CAnimator.h>
#include <ECS/Component/Types/CTransform/CTransform.h>
#include <ECS/ECSInternalComponents.h>
#include <Logger/Logger.h>
#include <ResourceManager/Types/ResourceSkeleton/ResourceSkeleton.h>
#include <Utils/Serialization/JsonObject.h>

#include <entt/entt.hpp>

#include <cstddef>
#include <vector>

JsonObject CBoneAttachment::Serialize() const
{
    JsonObject root;
    root.Set("type",     GetType());
    root.Set("boneName", boneName);
    return root;
}

void CBoneAttachment::Deserialize(const JsonObject& obj)
{
    boneName = obj.GetString("boneName");

    // A new name means any warning already emitted names a bone this component no
    // longer refers to.
    warnedUnresolved = false;
}

// ---------------------------------------------------------------------------
// Resolution
// ---------------------------------------------------------------------------

namespace
{
    entt::entity ParentOf(const entt::registry& registry, entt::entity entity)
    {
        const auto* hierarchy = registry.try_get<CHierarchy>(entity);
        return hierarchy ? hierarchy->parent : entt::null;
    }

    // The attached bone's MODEL-space global for the current pose, or nullptr when
    // anything along the way does not resolve. Warns at most once per component --
    // this runs per attached object per frame, so a plain warning would be a flood.
    const glm::mat4* ResolveBoneGlobal(const entt::registry& registry, entt::entity entity)
    {
        const auto* attachment = registry.try_get<CBoneAttachment>(entity);
        if (!attachment || attachment->boneName.empty())
            return nullptr;   // not an error: an unset slot is the default state

        const char* reason = "it has no ancestor with an Animator";

        for (entt::entity a = ParentOf(registry, entity); a != entt::null; a = ParentOf(registry, a))
        {
            const auto* animator = registry.try_get<CAnimator>(a);
            if (!animator)
                continue;

            // NEAREST ancestor wins: an animator found here settles the question even
            // when it cannot supply a matrix. Walking past it would silently attach the
            // prop to a grandparent character.
            if (!animator->skeleton)
            {
                reason = "its Animator has no skeleton assigned";
                break;
            }

            const int bone = animator->skeleton->skeleton.FindBone(attachment->boneName);
            if (bone < 0)
            {
                reason = "its Animator's skeleton has no bone by that name";
                break;
            }

            // Empty until a successful bind and sample -- the same "empty means not
            // usable" contract GetPalette() carries.
            const std::vector<glm::mat4>& globals = animator->GetBoneGlobals();
            if (static_cast<std::size_t>(bone) >= globals.size())
            {
                reason = "its Animator has not sampled a pose yet";
                break;
            }

            return &globals[bone];
        }

        if (!attachment->warnedUnresolved)
        {
            attachment->warnedUnresolved = true;
            NOUS_WARN("CBoneAttachment: cannot attach to bone '%s' because %s.",
                      attachment->boneName.c_str(), reason);
        }
        return nullptr;
    }
}

glm::mat4 ComputeParentWorld(const entt::registry& registry, entt::entity entity)
{
    glm::mat4 parentWorld(1.0f);

    if (const entt::entity parent = ParentOf(registry, entity); parent != entt::null)
        if (const auto* parentTransform = registry.try_get<CTransform>(parent))
            parentWorld = parentTransform->worldMatrix;

    if (const glm::mat4* boneGlobal = ResolveBoneGlobal(registry, entity))
        return parentWorld * (*boneGlobal);

    return parentWorld;
}
