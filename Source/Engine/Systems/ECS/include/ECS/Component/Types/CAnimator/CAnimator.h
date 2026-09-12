#pragma once

#include <ECS/Component/Component.h>
#include <AnimationSystem/AnimInstance.h>
#include <AnimationSystem/AnimParameters.h>
#include <AnimationSystem/Binding.h>
#include <AnimationSystem/Pose.h>
#include <AnimationSystem/RootMotion.h>
#include <EngineCore/EngineExport.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <string_view>
#include <vector>

class ResourceSkeleton;
class ResourceAnimation;
class ResourceAnimationController;

/**
 * @brief What happens to the travel baked into a clip's root bone.
 *
 * Baked is the DEFAULT so an existing scene renders exactly as it did before root
 * motion existed: turning this on is opt-in per character.
 *
 * An in-place export produces a zero delta, so it behaves identically in all
 * three modes -- which is why the engine needs no notion of "this clip is in
 * place". The asymmetry that shapes the whole feature: a travelling clip can be
 * turned into an in-place one exactly, by subtraction, but the reverse means
 * inventing data the exporter discarded.
 */
enum class RootMotionMode
{
    Baked,     // travel stays in the pose; the object does not move. Today's behaviour.
    Applied,   // travel is stripped from the pose and added to the object's transform.
    InPlace,   // travel is stripped and discarded.
};

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

    // The state machine. A character is `skeleton + controller` and nothing else;
    // the clips live on the controller's states, not here.
    //
    // AN ANIMATOR WITH NO CONTROLLER PLAYS NOTHING, as Unity's does. There is
    // deliberately no "just play the first clip" fallback -- it would make a missing
    // controller look like a working animator with the wrong animation, which is
    // harder to diagnose than a character standing still in bind pose.
    ResourceAnimationController* controller = nullptr;

    // The AUTHORED `loop` and `speed` are PER CLIP and live on
    // ResourceAnimation::settings, not here. An animator holding an idle that must
    // loop and an attack that must not is the ordinary case, and one flag on the
    // component cannot say both. OnUpdate seeds each track's AnimInstance from its
    // own clip's settings every frame, so an Inspector edit is live.

    // A runtime scale OVER that authored speed -- slow motion, or a run cycle
    // following input. Unity's split: per-state speed on the asset, Animator.speed on
    // the component. This is what the script API's SetSpeed/GetSpeed reach.
    //
    // It is here rather than on the resource because ResourceAnimation is SHARED:
    // slowing one character by writing into the clip would retime every other
    // character playing it. This is the ONLY per-character speed axis there is -- a
    // controller asset (MVP-F) is shared between characters exactly as a clip is, so
    // it cannot host one either.
    //
    // SERIALIZED, unlike `parameters`: "this character moves heavily" is authoring.
    // That does not reintroduce the deleted CAnimator::speed, which was ABSOLUTE and
    // so competed with each clip's own value; a multiplier composes with it and
    // applies uniformly to every clip by design.
    //
    // Per-character AND per-clip variation is MVP-F's job: a controller state's speed
    // driven by an AnimParameter. Do not grow a second multiplier here for it.
    float speedMultiplier = 1.0f;

    // Fade duration the Inspector's Play buttons use. Authoring convenience only --
    // Play() takes its duration as a parameter, so a future script API and a future
    // controller graph each supply their own.
    float fadeSeconds = 0.2f;

    // Authoring, unlike `parameters` -- serialized, because a character silently
    // reverting to Baked on load would look like the feature failing.
    RootMotionMode rootMotion = RootMotionMode::Baked;

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

    // The clip of the CURRENT STATE -- during a fade the INCOMING one, because the
    // destination becomes current the instant a transition starts (design §4). The
    // outgoing side is a pose with no state behind it, so it has no progress anyone
    // could act on. Null when nothing is bound.
    [[nodiscard]] NOUS_ENGINE_API const ResourceAnimation* CurrentClip() const;

    // Cross-fades to the named STATE of the controller's graph over fadeSeconds.
    // Returns false and changes nothing when there is no controller or no state has
    // that name.
    //
    // ARBITRATION (design §7): the graph evaluates every frame; a direct CrossFade
    // WINS for that frame and re-enters the graph at the named state. There is never
    // a frame with two writers -- a successful call suppresses the graph for exactly
    // the next OnUpdate, after which the graph evaluates normally from the state this
    // put the animator in. A REJECTED call suppresses nothing.
    //
    // fadeSeconds <= 0 snaps. Calling this while a fade is already running folds the
    // in-flight blend into the outgoing pose and starts a new fade from it, so the
    // animator never holds more than two tracks no matter how often this is called.
    NOUS_ENGINE_API bool CrossFade(std::string_view stateName, float fadeSeconds);

    // The graph's current state, or empty when there is no controller or the current
    // state does not resolve. Used by the Inspector readout, the controller editor's
    // active-state highlight, and the script API.
    //
    // NOT null-terminated -- it views the state's own name. Copy by length.
    [[nodiscard]] NOUS_ENGINE_API std::string_view GetCurrentStateName() const;

    [[nodiscard]] NOUS_ENGINE_API bool IsFading() const { return m_fadeDuration > 0.0f; }

    // 0..1 through the clip CurrentClip() names -- the INCOMING one during a fade.
    // Returns 0 when unbound or the clip has no duration.
    //
    // This is what a transition's exit time is measured against, which is the whole
    // reason it follows the incoming side: "when the attack finishes" has to mean the
    // attack, and the attack is the clip being faded IN. Both queries read the track
    // CurrentTrack() picks, so they cannot describe different clips. Pinned by
    // t_CAnimator.NormalizedTimeFollowsTheINCOMINGClipDuringAFade.
    [[nodiscard]] NOUS_ENGINE_API float GetNormalizedTime() const;

    // This frame's blended root motion, in the ANIMATION's space -- before the
    // object's orientation and scale are applied. Zero in Baked mode.
    //
    // Exposed for a future physics integration or controller, which would consume
    // this instead of letting the animator write the transform directly (Unity's
    // OnAnimatorMove seam). Deliberately NOT on the script API: nothing needs it
    // yet, and an unused published SDK surface is one that has to be kept.
    [[nodiscard]] NOUS_ENGINE_API const nous::engine::animation_system::RootMotionDelta&
    GetRootMotionDelta() const { return m_rootDelta; }

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

        // The root bone's transform as of the last sample, and its transform at the
        // clip's start and end. The endpoints never change for a bound clip, so they
        // are computed once in RebindTrack and make the loop-seam split pure
        // arithmetic. All three are meaningless when binding.rootBone < 0.
        nous::engine::animation_system::Transform previousRoot;
        nous::engine::animation_system::Transform rootAtStart;
        nous::engine::animation_system::Transform rootAtEnd;
    };

    // Rebuilds the track's binding from its clip and the animator's skeleton, and
    // sizes its pose. Clears the track when either side is null.
    void RebindTrack(ClipTrack& track);

    // Pushes the track's clip's authored loop/speed onto its AnimInstance, scaling
    // the speed by speedMultiplier. Called per frame for both tracks, so an Inspector
    // edit to the resource reaches a clip that is already playing. A null clip leaves
    // the instance alone -- it has nothing to sample anyway.
    void SeedPlaybackSettings(ClipTrack& track) const;

    // The ResourceAnimation a controller state plays, or null when there is no
    // controller, the index does not resolve, or that state has no clip assigned.
    [[nodiscard]] ResourceAnimation* ClipForState(int stateIndex) const;

    // Makes `stateIndex` current and starts the fade into its clip. THE ONLY WRITER
    // of m_currentState besides the controller bind -- CrossFade and the graph both
    // go through here, or the two paths drift.
    void EnterState(int stateIndex, float fadeSeconds);

    // The track carrying the CURRENT state's clip: m_to while a fade is running,
    // m_from otherwise. CurrentClip and GetNormalizedTime both go through this, which
    // is what makes it impossible for them to name different clips.
    [[nodiscard]] const ClipTrack& CurrentTrack() const
    { return (m_fadeDuration > 0.0f && m_to.clip) ? m_to : m_from; }

    // ---- ONE LAYER'S RUNTIME ----
    //
    // These six are the complete state of one animation layer. Layers are out of
    // scope, and the only thing keeping them cheap to add later is that this block
    // can be wrapped in a struct MECHANICALLY rather than untangled -- a layer is
    // one more instance of the same evaluator plus one more copy of exactly this.
    // Keep them together and do not interleave unrelated members.
    int                                  m_currentState = -1;
    ClipTrack                            m_from;
    ClipTrack                            m_to;
    nous::engine::animation_system::Pose m_blended;
    float                                m_fadeElapsed  = 0.0f;
    float                                m_fadeDuration = 0.0f;   // 0 == not fading
    // ---- end layer runtime ----

    // Set by a successful CrossFade, cleared at the END of the next OnUpdate. That
    // ordering is the arbitration rule itself: clearing at the end rather than at the
    // top is what makes the override last exactly one frame, and makes a CrossFade
    // called from anywhere -- a script's Update, an Inspector button, a future
    // animation event -- hold off the graph for the frame that follows it.
    //
    // NOT part of the layer runtime block above: arbitration is per animator, and a
    // second layer would share this flag rather than carry its own.
    bool m_graphSuppressedThisFrame = false;

    std::vector<glm::mat4> m_globals;
    std::vector<glm::mat4> m_palette;

    // UID the tracks' bindings were built against, same compare-every-frame rule.
    uint32_t m_boundSkeleton = 0;

    // UID and generation the graph was last bound against. Compared every frame --
    // the same compare-do-not-remember rule the skeleton slot uses, so every path
    // that can swap a controller (Inspector drop, Deserialize, the resource going
    // away, an editor re-save) is covered with no "remember to call Bind()" contract.
    uint32_t m_boundController = 0;
    uint32_t m_boundGeneration = 0;

    nous::engine::animation_system::RootMotionDelta m_rootDelta;

    // One warning per animator for Applied with no CTransform -- a real authoring
    // mistake, unlike a zero delta, which correct in-place input produces every frame.
    bool m_warnedNoTransform = false;

    // Pulls one track's delta and takes the travel out of its pose. Returns a zero
    // delta when the track has no root bone, is frozen, or the mode is Baked.
    nous::engine::animation_system::RootMotionDelta
    ExtractTrackRootMotion(ClipTrack& track, bool wrapped);

    // Adds m_rootDelta to the owning GameObject's transform. Applied mode only.
    void ApplyRootMotion();
};
