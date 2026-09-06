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
 * The consequence: there is no bone entity to parent a prop to. CBoneAttachment is
 * the mechanism that makes that affordable -- it injects the bone's global into
 * Scene::UpdateWorldMatrices for one object, instead of every joint costing an entity.
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

    // Set once ApplySkinningToGeometry has reported a mesh whose rig does not match
    // `skeleton`, so the warning is one per animator rather than one per mesh every
    // frame. Mutable because the pairing reads the animator through a const registry.
    // Cleared by Rebind(), so swapping the slot gives the next mistake its own warning.
    mutable bool       warnedSkeletonMismatch = false;

    NOUS_ENGINE_API void       OnUpdate(float deltaTime) override;
    NOUS_ENGINE_API JsonObject Serialize()               const override;
    NOUS_ENGINE_API void       Deserialize(const JsonObject& obj)  override;

    // Releases the skeleton and clip references, mirroring CMesh::OnDestroy. BOTH ways a
    // slot is filled take a reference -- Deserialize through CreateResource /
    // CreateResourceFromLibrary, and the Inspector's drag-drop through CreateResource --
    // so without this every play/stop cycle deserializes the scene again and leaks one
    // reference per slot per animator. The resources then never evict and the Resources
    // window shows counts climbing by the number of animators using them.
    NOUS_ENGINE_API void       OnDestroy()                         override;

    // MODEL-space bone globals for the current pose, parallel to the skeleton's
    // bone array. Empty until a successful bind + sample. Compose with the owning
    // GameObject's world matrix to reach world space -- which is what the Scene
    // View's debug draw does, and what the GPU skinning palette will do.
    [[nodiscard]] NOUS_ENGINE_API const std::vector<glm::mat4>& GetBoneGlobals() const
    { return m_globals; }

    // The GPU skinning palette for the current pose: palette[b] takes a vertex from
    // mesh space into that bone's animated place, in MODEL space. Composed with the
    // owning GameObject's world matrix to reach world space.
    //
    // EMPTY MEANS "NOT USABLE", and the renderer's skinned-geometry test is exactly
    // `!GetPalette().empty()`. So it is cleared whenever a slot is cleared or the
    // build fails -- a stale palette would silently deform a mesh to a pose that no
    // longer has a source, which reads as a skinning bug rather than a binding one.
    [[nodiscard]] NOUS_ENGINE_API const std::vector<glm::mat4>& GetPalette() const
    { return m_palette; }

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
    std::vector<glm::mat4>                           m_palette;

    // UIDs m_binding was built from; compared against the slots every frame, which
    // is what makes a slot change rebind without an explicit call from the editor.
    uint32_t m_boundClip     = 0;
    uint32_t m_boundSkeleton = 0;
};
