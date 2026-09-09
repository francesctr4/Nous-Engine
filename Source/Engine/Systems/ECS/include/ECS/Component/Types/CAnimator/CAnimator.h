#pragma once

#include <ECS/Component/Component.h>
#include <AnimationSystem/AnimInstance.h>
#include <AnimationSystem/AnimParameters.h>
#include <AnimationSystem/Binding.h>
#include <AnimationSystem/Pose.h>
#include <EngineCore/EngineExport.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <string_view>
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

    ResourceSkeleton* skeleton = nullptr;   // .nskel -- the rig

    // The clips this animator can play, authored in the Inspector. Index 0 is what
    // plays until something calls Play(). Lookup is by RESOURCE name, never
    // AnimClipData::name -- every Mixamo export calls its clip "mixamo.com".
    std::vector<ResourceAnimation*> clips;

    float speed = 1.0f;      // negative plays backwards
    bool  loop  = true;

    // Fade duration the Inspector's Play buttons use. Authoring convenience only --
    // Play() takes its duration as a parameter, so a future script API and a future
    // controller graph each supply their own.
    float fadeSeconds = 0.2f;

    // Named values scripts write and the controller graph (MVP-F) reads. Runtime
    // state, deliberately NOT serialized -- defaults belong in the controller asset
    // once one exists. Survives script hot-reload, which recreates script instances
    // but not components.
    nous::engine::animation_system::AnimParameters parameters;

    // Set once ApplySkinningToGeometry has reported a mesh whose rig does not match
    // `skeleton`, so the warning is one per animator rather than one per mesh every
    // frame. Mutable because the pairing reads the animator through a const registry.
    // Cleared on a skeleton swap, so the next mistake gets its own warning.
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
    { return m_from.boundClip != 0 && m_boundSkeleton != 0; }

    // The clip currently driving the pose. During a fade this is the OUTGOING clip;
    // it becomes the incoming one when the fade completes. Null when nothing is bound.
    [[nodiscard]] NOUS_ENGINE_API const ResourceAnimation* CurrentClip() const;

    // Cross-fades to the clip in `clips` whose RESOURCE name matches, over
    // fadeSeconds. Returns false and changes nothing when no clip matches.
    //
    // ARBITRATION, decided with the scripting API and to be honoured by the
    // controller graph (MVP-F): the graph evaluates every frame; a direct Play wins
    // for that frame and re-enters the graph at the named state. There is never a
    // frame with two writers. With no graph, Play behaves exactly as below.
    //
    // fadeSeconds <= 0 snaps. Calling this while a fade is already running folds the
    // in-flight blend into the outgoing pose and starts a new fade from it, so the
    // animator never holds more than two tracks no matter how often this is called.
    NOUS_ENGINE_API bool Play(std::string_view clipName, float fadeSeconds);

    [[nodiscard]] NOUS_ENGINE_API bool IsFading() const { return m_fadeDuration > 0.0f; }

    // 0..1 through the CURRENT clip -- the one CurrentClip() names, which during a
    // fade is the OUTGOING one. Returns 0 when unbound or the clip has no duration.
    //
    // The fade case is a known wart, accepted rather than fixed: the fix is a second
    // query whose meaning changes once the controller graph lands, and normalized
    // time is mostly a pre-graph idiom -- afterwards the idiom is a trigger plus a
    // transition with exit time. Pinned by
    // t_CAnimator.NormalizedTimeFollowsTheOutgoingClipDuringAFade.
    [[nodiscard]] NOUS_ENGINE_API float GetNormalizedTime() const;

private:
    // One playing clip plus everything needed to sample it. Two of these is the whole
    // blend model -- a re-trigger folds the in-flight blend into m_from rather than
    // adding a third track, so the animator is bounded at two under any input.
    struct ClipTrack
    {
        ResourceAnimation*                               clip      = nullptr;
        nous::engine::animation_system::AnimInstance     instance;
        nous::engine::animation_system::AnimationBinding binding;
        nous::engine::animation_system::Pose             pose;

        // UID `binding` was built from; compared against `clip` every frame, which is
        // what makes a slot change rebind without an explicit call from the editor.
        uint32_t                                         boundClip = 0;

        // The pose is fixed: do not advance or sample it. Set only when a re-trigger
        // captures an in-flight blend as the new source.
        bool                                             frozen    = false;
    };

    // Rebuilds the track's binding from its clip and the animator's skeleton, and
    // sizes its pose. Clears the track when either side is null.
    void RebindTrack(ClipTrack& track);

    ClipTrack                            m_from;
    ClipTrack                            m_to;
    nous::engine::animation_system::Pose m_blended;

    std::vector<glm::mat4> m_globals;
    std::vector<glm::mat4> m_palette;

    // UID the tracks' bindings were built against, same compare-every-frame rule.
    uint32_t m_boundSkeleton = 0;

    float m_fadeElapsed  = 0.0f;
    float m_fadeDuration = 0.0f;   // 0 == not fading
};
