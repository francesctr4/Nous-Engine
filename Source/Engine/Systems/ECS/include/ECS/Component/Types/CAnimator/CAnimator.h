#pragma once

#include <ECS/Component/Component.h>
#include <AnimationSystem/Clip/AnimInstance.h>
#include <AnimationSystem/Controller/AnimParameters.h>
#include <AnimationSystem/Sampling/Binding.h>
#include <AnimationSystem/Pose.h>
#include <AnimationSystem/RootMotion/RootMotion.h>
#include <EngineCore/EngineExport.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

class ResourceSkeleton;
class ResourceAnimation;
class ResourceAnimationController;

/** @brief What happens to the travel baked into a clip's root bone. */
enum class RootMotionMode
{
    // FIRST, so 0 == Inherit and ControllerState::rootMotion's int default means "use the
    // component's mode" with no conversion table. Only valid on a controller state; on the
    // component itself it behaves as Baked.
    Inherit,

    Baked,     // travel stays in the pose; the object does not move
    Applied,   // travel is stripped from the pose and added to the object's transform
    InPlace,   // travel is stripped and discarded
};

/**
 * @brief Plays one animation clip against one skeleton, into an internal pose.
 *
 * The pose is internal -- bones are not GameObjects, so animation output never flows
 * through the ECS transform hierarchy. CBoneAttachment is how a prop reaches a bone.
 *
 * Time comes from Scene::Update's simDt, so playback is pausable and steppable with no
 * ISceneHost query. Unlike CAudioSource this watches no sim-state edges: a pose has no
 * lifecycle, and a stopped scene simply displays t = 0.
 *
 * Both resource slots are assigned by the user; nothing on disk says a mesh is rigged to
 * a given .nskel. Mirrors Unity's Animator (Avatar + Controller).
 */
class CAnimator : public Component {
public:
    COMPONENT_TYPE(CAnimator)

    ResourceSkeleton* skeleton = nullptr;   // .nskel -- the rig

    // The state machine, and the only source of clips. An animator with no controller
    // plays nothing, as Unity's does -- a "play the first clip" fallback would make a
    // missing controller look like a working animator with the wrong animation.
    ResourceAnimationController* controller = nullptr;

    // Runtime scale OVER each clip's authored speed (ResourceAnimation::settings), and the
    // only per-character speed axis: both the clip and the controller are shared assets, so
    // neither can host one. SeedPlaybackSettings composes this with the clip's speed, the
    // state's speed and the state's speed parameter.
    float speedMultiplier = 1.0f;

    // Fade duration the Inspector's Play buttons use. Every other caller supplies its own.
    float fadeSeconds = 0.2f;

    RootMotionMode rootMotion = RootMotionMode::Baked;

    // Named values the graph's transition conditions read, written by scripts and the
    // Inspector panel. Runtime state, NOT serialized -- the defaults are authored on the
    // controller asset, which is the one place they apply to every character using it.
    nous::engine::animation_system::AnimParameters parameters;

    // One warning per animator, not one per mesh per frame. Mutable because the pairing
    // check reads the animator through a const registry; cleared on a skeleton swap.
    mutable bool       warnedSkeletonMismatch = false;

    NOUS_ENGINE_API void       OnUpdate(float deltaTime) override;
    NOUS_ENGINE_API JsonObject Serialize()               const override;
    NOUS_ENGINE_API void       Deserialize(const JsonObject& obj)  override;

    // Releases the skeleton and clip references, mirroring CMesh::OnDestroy. Both ways a
    // slot is filled take a reference, so without this every play/stop cycle leaks one.
    NOUS_ENGINE_API void       OnDestroy()                         override;

    // MODEL-space bone globals for the current pose, parallel to the skeleton's bone array.
    // Compose with the GameObject's world matrix to reach world space.
    [[nodiscard]] NOUS_ENGINE_API const std::vector<glm::mat4>& GetBoneGlobals() const
    { return m_globals; }

    // GPU skinning palette: palette[b] takes a vertex from mesh space into that bone's
    // animated place, in MODEL space.
    //
    // EMPTY MEANS "NOT USABLE" -- the renderer's skinned-geometry test is exactly
    // !GetPalette().empty(), so it is cleared whenever a slot is cleared or a build fails.
    [[nodiscard]] NOUS_ENGINE_API const std::vector<glm::mat4>& GetPalette() const
    { return m_palette; }

    [[nodiscard]] NOUS_ENGINE_API bool IsBound() const
    { return m_from.boundClip != 0 && m_boundSkeleton != 0; }

    // The current state's clip -- during a fade the INCOMING one, since the destination
    // becomes current the instant a transition starts. Null when unbound.
    [[nodiscard]] NOUS_ENGINE_API const ResourceAnimation* CurrentClip() const;

    // Cross-fades to the named state of the controller's graph. False if there is no
    // controller or no state by that name.
    //
    // Arbitration: the graph evaluates every frame, but a successful call here
    // suppresses it for exactly the next OnUpdate, so there is never a frame with two
    // writers. A rejected call suppresses nothing. fadeSeconds <= 0 snaps.
    NOUS_ENGINE_API bool CrossFade(std::string_view stateName, float fadeSeconds);

    // NOT null-terminated -- it views the state's own name. Copy by length.
    [[nodiscard]] NOUS_ENGINE_API std::string_view GetCurrentStateName() const;

    [[nodiscard]] NOUS_ENGINE_API bool IsFading() const { return m_fadeDuration > 0.0f; }

    // 0..1 through the current cross-fade; 0 when not fading. A fade advances on simDt, so
    // one armed while the scene is STOPPED sits at 0 -- which is what the Inspector readout
    // distinguishes from a fade that is simply fast.
    [[nodiscard]] NOUS_ENGINE_API float GetFadeProgress() const
    { return m_fadeDuration > 0.0f ? glm::clamp(m_fadeElapsed / m_fadeDuration, 0.0f, 1.0f) : 0.0f; }

    // 0..1 through CurrentClip() -- the incoming clip during a fade, because that is what a
    // transition's exit time has to measure. 0 when unbound or the clip has no duration.
    [[nodiscard]] NOUS_ENGINE_API float GetNormalizedTime() const;

    // This frame's blended root motion, in the ANIMATION's space -- before the object's
    // orientation and scale. Non-zero only for a track resolved to Applied.
    //
    // The seam a physics integration would consume instead of letting the animator write the
    // transform (Unity's OnAnimatorMove). Deliberately not on the script API.
    [[nodiscard]] NOUS_ENGINE_API const nous::engine::animation_system::RootMotionDelta&
    GetRootMotionDelta() const { return m_rootDelta; }

    // EDITOR ONLY. Arms a pose preview: the next OnUpdate seeks `clip` to `time` and
    // rebuilds the globals and palette after its normal work. A null clip disarms.
    //
    // It ARMS rather than samples because the render packet is built from GetPalette()
    // before editor windows draw, so a pose written at draw time appears one frame late.
    //
    // THE ARM LASTS ONE OnUpdate and must be renewed every frame while scrubbing -- a
    // latched preview could never be taken back, since a closed window stops being called at
    // all. It samples through a local binding/instance/pose, so it cannot disturb playback
    // or an in-flight fade. Fires no events and applies no root motion.
    NOUS_ENGINE_API void SetPreview(const ResourceAnimation* clip, float time);

private:
    // One playing clip plus everything needed to sample it. Two of these is the whole blend
    // model: a re-trigger folds the in-flight blend into m_from rather than adding a third.
    struct ClipTrack
    {
        ResourceAnimation*                               clip      = nullptr;
        nous::engine::animation_system::AnimInstance     instance;
        nous::engine::animation_system::AnimationBinding binding;
        nous::engine::animation_system::Pose             pose;

        // UID `binding` was built from, compared against `clip` every frame -- which is what
        // makes a slot change rebind with no explicit call from the editor.
        uint32_t                                         boundClip = 0;

        // The pose is fixed: do not advance or sample it. Set only by a re-trigger capturing
        // an in-flight blend as the new source.
        bool                                             frozen    = false;

        // The state this track was entered from, and that state's RESOLVED mode. Both fixed
        // at enter, not read per frame, because during a cross-fade the two tracks routinely
        // disagree -- which is what lets an outgoing Applied state fade out against an
        // incoming InPlace one.
        int                                              stateIndex = -1;
        RootMotionMode                                   mode       = RootMotionMode::Baked;

        // The root bone as of the last sample, plus its transform at the clip's start and
        // end. The endpoints never change for a bound clip, so they are computed once in
        // RebindTrack and make the loop-seam split pure arithmetic. All three are
        // meaningless when binding.rootBone < 0.
        nous::engine::animation_system::Transform previousRoot;
        nous::engine::animation_system::Transform rootAtStart;
        nous::engine::animation_system::Transform rootAtEnd;
    };

    // Rebuilds the track's binding and sizes its pose. Clears it when either side is null.
    void RebindTrack(ClipTrack& track);

    // Moves a bound track's cursor AND re-establishes previousRoot at that time. The second
    // half is the whole reason this is a function: RebindTrack leaves previousRoot at the
    // clip's start, so without it the next frame measures travel from t = 0 and moves an
    // Applied character by most of a clip in one frame. Any future caller that repositions a
    // playing track must come through here.
    void SeekTrackTo(ClipTrack& track, float time);

    // Composes the track's playback rate from four factors -- the clip's authored speed, the
    // state's speed, the state's optional speed parameter, and speedMultiplier -- and pushes
    // the clip's authored loop. Per frame for both tracks, so an Inspector edit or a script
    // writing the parameter reaches a clip that is already playing.
    void SeedPlaybackSettings(ClipTrack& track) const;

    // Gives every parameter the controller DECLARES a value without disturbing one the
    // animator already holds, so a script's value survives the editor re-saving the asset.
    void SeedDeclaredParameters();

    // The state's own mode, or the component's when the state says Inherit or does not
    // resolve.
    [[nodiscard]] RootMotionMode ResolveRootMotion(int stateIndex) const;

    [[nodiscard]] ResourceAnimation* ClipForState(int stateIndex) const;

    // Makes `stateIndex` current and starts the fade into its clip. THE ONLY WRITER of
    // m_currentState besides the controller bind -- CrossFade and the graph both come
    // through here, or the two paths drift.
    void EnterState(int stateIndex, float fadeSeconds);

    // The track carrying the current state's clip. CurrentClip and GetNormalizedTime both go
    // through this, so they cannot name different clips.
    [[nodiscard]] const ClipTrack& CurrentTrack() const
    { return (m_fadeDuration > 0.0f && m_to.clip) ? m_to : m_from; }

    // What the GRAPH is told about progress, which is not what GetNormalizedTime() reports.
    // A state whose clip did not resolve is trivially finished and reports 1.0, so an
    // exit-time edge can still leave it; the public getter keeps returning 0.0 rather than
    // telling a script a clip finished when there was no clip.
    [[nodiscard]] float GraphProgress() const;

    // ---- ONE LAYER'S RUNTIME ----
    // The complete state of one animation layer. Layers are out of scope; keeping this block
    // together is what would let one be wrapped in a struct mechanically rather than
    // untangled. Do not interleave unrelated members.
    int                                  m_currentState = -1;

    // The name of m_currentState, remembered at enter rather than looked up: an index is only
    // meaningful against the graph it was resolved in, and a re-save replaces that graph in
    // place. This is what "preserve by name across a re-save" preserves FROM.
    std::string                          m_currentStateName;
    ClipTrack                            m_from;
    ClipTrack                            m_to;
    nous::engine::animation_system::Pose m_blended;
    float                                m_fadeElapsed  = 0.0f;
    float                                m_fadeDuration = 0.0f;   // 0 == not fading
    // ---- end layer runtime ----

    // Set by a successful CrossFade, cleared at the END of the next OnUpdate -- that ordering
    // is the arbitration rule itself. Per animator, not per layer.
    bool m_graphSuppressedThisFrame = false;

    // Armed by SetPreview, consumed at the end of OnUpdate. Per animator, not per layer.
    const ResourceAnimation* m_previewClip = nullptr;
    float                    m_previewTime = 0.0f;

    // Samples m_previewClip through a scratch binding + instance + pose, leaving every track
    // untouched.
    void ApplyPreview();

    std::vector<glm::mat4> m_globals;
    std::vector<glm::mat4> m_palette;

    uint32_t m_boundSkeleton = 0;

    // UID and generation the graph was last bound against, compared every frame -- the same
    // compare-do-not-remember rule the skeleton slot uses, so every path that can swap a
    // controller is covered with no "remember to call Bind()" contract.
    uint32_t m_boundController = 0;
    uint32_t m_boundGeneration = 0;

    nous::engine::animation_system::RootMotionDelta m_rootDelta;

    // One warning per animator for Applied with no CTransform -- a real authoring mistake,
    // unlike a zero delta, which correct in-place input produces every frame.
    bool m_warnedNoTransform = false;

    // Pulls one track's delta and takes the travel out of its pose. Zero when the track has
    // no root bone, is frozen, or the mode is Baked.
    nous::engine::animation_system::RootMotionDelta
    ExtractTrackRootMotion(ClipTrack& track, bool wrapped);

    // Adds m_rootDelta to the owning GameObject's transform. Applied mode only.
    void ApplyRootMotion();

    // Fires the events the track crossed this frame, in order, to every script on the
    // GameObject. `timeBefore` must be the instance's time BEFORE Advance and `wrapped`
    // Advance's return value -- the loop seam is where a naive interval is wrong.
    void FireTrackEvents(const ClipTrack& track, float timeBefore, bool wrapped);
};
