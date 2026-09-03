#pragma once

#include <ECS/Component/Component.h>
#include <AnimationSystem/AnimInstance.h>
#include <AnimationSystem/Binding.h>
#include <AnimationSystem/Pose.h>
#include <EngineCore/EngineExport.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

class ResourceSkeleton;
class ResourceAnimation;

/**
 * @brief Plays one animation clip against one skeleton, into an internal pose.
 *
 * THE POSE IS INTERNAL. Bones are not GameObjects and animation output never
 * flows through the ECS transform hierarchy -- a 66-joint Mixamo rig would
 * otherwise cost 66 entities per character, and every one of them would be
 * walked by Scene::UpdateWorldMatrices every frame.
 *
 * The consequence, accepted deliberately: there is no bone entity to parent a
 * prop to, so sockets/attachments have no mechanism. Deferred until after GPU
 * skinning lands, at which point the cost/benefit is judgeable against a
 * character that visibly moves.
 *
 * The two resource slots are assigned by the user (Inspector drag-drop), NOT
 * inferred from a sibling CMesh: nothing on disk or in memory says a mesh is
 * rigged to a given .nskel, and an anim-only Mixamo FBX's clips must bind to
 * another file's rig anyway. This mirrors Unity's Animator (Avatar + Controller).
 *
 * TIME comes from Scene::Update's simDt, which is 0 when STOPPED, dt when
 * PLAYING and exactly one frame's worth on a PAUSED single-step. So playback is
 * correctly pausable and steppable with no ISceneHost query at all. Unlike
 * CAudioSource, this component watches no simulation-state edges, because a pose
 * has no lifecycle to start or release -- Advance(instance, 0) is a no-op and a
 * stopped scene simply displays the pose at t = 0.
 */
class CAnimator : public Component {
public:
    COMPONENT_TYPE(CAnimator)

    ResourceSkeleton*  skeleton = nullptr;   // .nskel -- the rig
    ResourceAnimation* clip     = nullptr;   // .nanim -- the clip to play
    float              speed    = 1.0f;      // negative plays backwards
    bool               loop     = true;

    NOUS_ENGINE_API void       OnUpdate(float deltaTime) override;
    NOUS_ENGINE_API JsonObject Serialize()               const override;
    NOUS_ENGINE_API void       Deserialize(const JsonObject& obj)  override;

    // MODEL-space bone globals for the current pose, parallel to the skeleton's
    // bone array. Empty until a successful bind + sample. Compose with the owning
    // GameObject's world matrix to reach world space -- which is what the Scene
    // View's debug draw does, and what the GPU skinning palette will do.
    [[nodiscard]] NOUS_ENGINE_API const std::vector<glm::mat4>& GetBoneGlobals() const
    { return m_globals; }

    [[nodiscard]] NOUS_ENGINE_API bool IsBound() const
    { return m_boundClip != 0 && m_boundSkeleton != 0; }

private:
    // Rebuilds m_binding from the current slots and preallocates the pose and
    // globals buffers. Clears everything when either slot is null.
    void Rebind();

    nous::engine::animation_system::AnimInstance     m_instance;
    nous::engine::animation_system::AnimationBinding m_binding;
    nous::engine::animation_system::Pose             m_pose;
    std::vector<glm::mat4>                           m_globals;

    // UIDs m_binding was built from; compared against the slots every frame, which
    // is what makes a slot change rebind without an explicit call from the editor.
    uint32_t m_boundClip     = 0;
    uint32_t m_boundSkeleton = 0;
};
