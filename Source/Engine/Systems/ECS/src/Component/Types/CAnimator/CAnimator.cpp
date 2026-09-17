#include <ECS/Component/Types/CAnimator/CAnimator.h>

#include <AnimationSystem/Events/AnimationEvents.h>
#include <AnimationSystem/Sampling/Blending.h>
#include <AnimationSystem/Controller/Controller.h>
#include <AnimationSystem/Skinning/Palette.h>
#include <AnimationSystem/Sampling/Sampling.h>
#include <AnimationSystem/RootMotion/RootMotion.h>
#include <EngineCore/Casts.h>
#include <ECS/Component/Types/CScript/CScript.h>
#include <ECS/Component/Types/CTransform/CTransform.h>
#include <ECS/ComponentServices.h>
#include <ECS/GameObject.h>
#include <FileSystem/FileSystem.h>   // GetFilename
#include <Logger/Logger.h>
#include <ResourceManager/Core/IResourceLoader.h>
#include <ResourceManager/Core/ResourceBase.h>
#include <ResourceManager/Types/ResourceAnimation/ResourceAnimation.h>
#include <ResourceManager/Types/ResourceAnimationController/ResourceAnimationController.h>
#include <ResourceManager/Types/ResourceSkeleton/ResourceSkeleton.h>
#include <ResourceManager/Types/ResourceType.h>
#include <Utils/Serialization/JsonArray.h>
#include <Utils/Serialization/JsonObject.h>

#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>

namespace anim = nous::engine::animation_system;

namespace
{
    uint32_t UIDOf(const ResourceBase* resource) { return resource ? resource->GetUID() : 0u; }

    // Serialized as a STRING, like CLight's enums: a numeric value would silently change
    // meaning if a mode were ever inserted mid-enum.
    const char* RootMotionToString(const RootMotionMode m)
    {
        switch (m)
        {
            case RootMotionMode::Inherit: return "Inherit";
            case RootMotionMode::Applied: return "Applied";
            case RootMotionMode::InPlace: return "InPlace";
            case RootMotionMode::Baked:   // fallthrough
            default:                      return "Baked";
        }
    }

    RootMotionMode RootMotionFromString(const std::string& s)
    {
        if (s == "Inherit") return RootMotionMode::Inherit;
        if (s == "Applied") return RootMotionMode::Applied;
        if (s == "InPlace") return RootMotionMode::InPlace;

        return RootMotionMode::Baked;   // unrecognised, including a scene predating Inherit
    }
}

// ---------------------------------------------------------------------------
// Binding
// ---------------------------------------------------------------------------

const ResourceAnimation* CAnimator::CurrentClip() const { return CurrentTrack().clip; }

float CAnimator::GetNormalizedTime() const
{
    const ClipTrack& track = CurrentTrack();

    if (!track.clip || track.boundClip == 0)
        return 0.0f;

    const float duration = track.clip->clip.duration;
    if (duration <= 0.0f)
        return 0.0f;

    return track.instance.time / duration;
}

float CAnimator::GraphProgress() const
{
    // Same predicate as GetNormalizedTime's guard, opposite answer: a state with no clip
    // is finished rather than at zero, so an exit-time edge out of it can fire.
    const ClipTrack& track = CurrentTrack();

    if (!track.clip || track.boundClip == 0)
        return 1.0f;

    return GetNormalizedTime();
}

ResourceAnimation* CAnimator::ClipForState(const int stateIndex) const
{
    if (!controller || !controller->graph.IsValidState(stateIndex))
        return nullptr;

    // clipIndex, never the state index: they coincide today only because the importer
    // fills one clip slot per state, and it is -1 for a state whose clip did not resolve.
    const int clipIndex = controller->graph.states[stateIndex].clipIndex;
    if (clipIndex < 0 || static_cast<size_t>(clipIndex) >= controller->clips.size())
        return nullptr;

    return controller->clips[clipIndex];
}

void CAnimator::SeedDeclaredParameters()
{
    if (!controller) return;

    for (const anim::ParameterDecl& decl : controller->graph.parameters)
    {
        // Contains is the only thing that can state absence: every getter returns its
        // fallback for a CROSS-TYPE entry as well as a missing one, so no sentinel can
        // tell "not held" from "held as a Bool".
        if (parameters.Contains(decl.name))
            continue;

        switch (static_cast<anim::AnimParamType>(decl.type))
        {
            case anim::AnimParamType::Float:
                parameters.SetFloat(decl.name, decl.defaultValue);
                break;

            case anim::AnimParamType::Bool:
                parameters.SetBool(decl.name, decl.defaultValue != 0.0f);
                break;

            case anim::AnimParamType::Trigger:
                break;   // a trigger has no default: it is set or it is not
        }
    }
}

RootMotionMode CAnimator::ResolveRootMotion(const int stateIndex) const
{
    if (!controller || !controller->graph.IsValidState(stateIndex))
        return rootMotion;

    const auto stateMode =
        static_cast<RootMotionMode>(controller->graph.states[stateIndex].rootMotion);

    return stateMode == RootMotionMode::Inherit ? rootMotion : stateMode;
}

void CAnimator::EnterState(const int stateIndex, const float fadeSeconds)
{
    // The destination becomes current IMMEDIATELY: the outgoing side is a
    // pose, not a state, which is what makes exit time measure the incoming clip.
    m_currentState = stateIndex;

    // Captured while the index still refers to the graph it was resolved in; deriving it
    // later is what a re-save breaks.
    m_currentStateName.assign(GetCurrentStateName());

    ResourceAnimation* target = ClipForState(stateIndex);

    // A FADE WITH NOTHING AT EITHER END SNAPS, and both halves are load-bearing. OnUpdate's
    // fade branch is gated on m_to.boundClip and sits behind the !IsBound() return, which
    // asks only about m_from -- so a fade to or from a clipless state would never advance,
    // never clear and never render: stuck fading, the per-frame re-derive of m_from
    // suppressed, and GraphProgress reading the wrong track so no exit-time edge could fire
    // again. Blending could not help anyway; an empty pose fails ArePosesCompatible.
    //
    // A FROZEN track is not sourceless -- it carries a re-trigger's captured blend, a real
    // pose with no clip behind it. Testing boundClip alone would snap every interruption.
    const bool nothingToFadeFrom = (m_from.boundClip == 0 && !m_from.frozen);

    if (fadeSeconds <= 0.0f || !target || nothingToFadeFrom)
    {
        m_from.clip       = target;
        m_from.stateIndex = stateIndex;
        m_from.mode       = ResolveRootMotion(stateIndex);
        RebindTrack(m_from);

        m_to.clip       = nullptr;
        m_to.stateIndex = -1;
        m_to.mode       = RootMotionMode::Baked;
        RebindTrack(m_to);

        m_fadeElapsed  = 0.0f;
        m_fadeDuration = 0.0f;
        return;
    }

    // Already fading: fold the current blended pose into the outgoing track and fade from
    // there, which is what keeps the animator at two tracks under arbitrary re-triggering.
    // RebindTrack clears `frozen`, so the capture must happen BEFORE rebinding the target,
    // and m_from must never be rebound on this path.
    if (m_fadeDuration > 0.0f)
    {
        m_from.pose   = m_blended;
        m_from.frozen = true;
    }

    m_to.clip       = target;
    m_to.stateIndex = stateIndex;
    m_to.mode       = ResolveRootMotion(stateIndex);
    RebindTrack(m_to);

    m_fadeElapsed  = 0.0f;
    m_fadeDuration = fadeSeconds;
}

bool CAnimator::CrossFade(const std::string_view stateName, const float fadeSeconds)
{
    if (!controller)
        return false;

    const int target = controller->graph.FindState(stateName);
    if (!controller->graph.IsValidState(target))
        return false;

    EnterState(target, fadeSeconds);

    // Set only on success: a call naming a state that does not exist must not cost the
    // graph a frame, or a typo reads as the state machine intermittently stalling.
    m_graphSuppressedThisFrame = true;
    return true;
}

std::string_view CAnimator::GetCurrentStateName() const
{
    if (!controller || !controller->graph.IsValidState(m_currentState))
        return {};

    return controller->graph.states[m_currentState].name;
}

void CAnimator::SeedPlaybackSettings(ClipTrack& track) const
{
    if (!track.clip) return;

    track.instance.loop = track.clip->settings.loop;

    // Four independent axes, composed as a product: per clip, per state, per state
    // following input, per character. Nothing here overrides anything.
    float rate = track.clip->settings.speed * speedMultiplier;

    if (controller && controller->graph.IsValidState(track.stateIndex))
    {
        const auto& state = controller->graph.states[track.stateIndex];
        rate *= state.speed;

        // Fallback 1.0f, not AnimParameters' own 0.0f: the rate is a product, so reading
        // zero for a parameter no script has written yet would freeze the character.
        if (!state.speedParameter.empty())
            rate *= parameters.GetFloat(state.speedParameter, 1.0f);
    }

    track.instance.speed = rate;
}

void CAnimator::SeekTrackTo(ClipTrack& track, const float time)
{
    if (!track.clip || track.boundClip == 0 || !skeleton) return;

    // Clamped: the clip may have been re-imported shorter while the animator held a time
    // inside the old duration.
    const float duration = track.clip->clip.duration;
    track.instance.Seek(duration > 0.0f ? glm::clamp(time, 0.0f, duration) : 0.0f);

    // previousRoot MUST follow the cursor -- RebindTrack leaves it at the clip's start, so
    // without this the next frame measures travel from t = 0 and hands an Applied character
    // most of a clip's displacement in one frame.
    if (track.binding.rootBone < 0) return;

    anim::AnimInstance probe = track.instance;
    probe.binding = &track.binding;

    anim::Pose probePose;
    anim::Sample(probe, skeleton->skeleton, m_boundSkeleton, probePose);

    if (static_cast<size_t>(track.binding.rootBone) < probePose.bones.size())
        track.previousRoot = probePose.bones[track.binding.rootBone];
}

void CAnimator::RebindTrack(ClipTrack& track)
{
    track.boundClip = UIDOf(track.clip);
    track.frozen    = false;

    if (!track.clip || !skeleton)
    {
        track.binding = {};
        track.pose    = {};
        track.instance.SetClip(nullptr, 0, nullptr);
        track.boundClip = 0;
        return;
    }

    track.binding = anim::CreateBinding(track.clip->clip, track.boundClip,
                                        skeleton->skeleton, m_boundSkeleton);

    track.instance.SetClip(&track.clip->clip, track.boundClip, &track.binding);

    // Preallocate here rather than letting Sample() size it on its first call.
    track.pose.skeleton = m_boundSkeleton;
    track.pose.bones.assign(skeleton->skeleton.BoneCount(), anim::Transform{});

    // The root at t=0 and t=duration, sampled once so the loop-seam split costs nothing per
    // frame. Sampling the whole skeleton twice is free in practice -- binding is rare, and
    // reusing the tested sampler beats a bespoke single-channel path.
    track.previousRoot = anim::Transform{};
    track.rootAtStart  = anim::Transform{};
    track.rootAtEnd    = anim::Transform{};

    if (track.binding.rootBone >= 0)
    {
        anim::AnimInstance probe = track.instance;
        probe.binding = &track.binding;

        anim::Pose probePose;

        probe.Seek(0.0f);
        anim::Sample(probe, skeleton->skeleton, m_boundSkeleton, probePose);
        track.rootAtStart = probePose.bones[track.binding.rootBone];

        probe.Seek(track.clip->clip.duration);
        anim::Sample(probe, skeleton->skeleton, m_boundSkeleton, probePose);
        track.rootAtEnd = probePose.bones[track.binding.rootBone];

        track.previousRoot = track.rootAtStart;   // the instance is at t=0 after a rebind
    }
}

// ---------------------------------------------------------------------------
// Root motion
// ---------------------------------------------------------------------------

anim::RootMotionDelta CAnimator::ExtractTrackRootMotion(ClipTrack& track, const bool wrapped)
{
    // The TRACK's resolved mode, never the component's: during a cross-fade the two
    // routinely disagree, which is what fades an outgoing Applied state's travel out
    // against an incoming InPlace one instead of snapping it off.
    if (track.mode != RootMotionMode::Applied && track.mode != RootMotionMode::InPlace)
        return {};

    if (track.frozen || track.binding.rootBone < 0) return {};
    if (static_cast<size_t>(track.binding.rootBone) >= track.pose.bones.size()) return {};

    // bindLocals too, since the strip target is read out of it below. Sample() treats a
    // short bindLocals as "no bind pose" rather than a precondition, so the two must agree
    // on what is guaranteed. Every importer fills it; a hand-built rig need not.
    if (static_cast<size_t>(track.binding.rootBone) >= skeleton->skeleton.bindLocals.size())
        return {};

    // BY VALUE: StripRootMotion mutates this very bone, so a reference would be read back
    // already stripped and every frame after the first would measure zero travel.
    const anim::Transform current = track.pose.bones[track.binding.rootBone];

    // The COMPOSED rate decides direction: a state speed of -1 over a forward clip plays it
    // backwards just as an authored -1 does, and the seam splits the way it is moving.
    const anim::RootMotionDelta delta = anim::ComputeRootDelta(
        track.previousRoot, current, track.rootAtStart, track.rootAtEnd, wrapped,
        track.instance.speed < 0.0f);

    track.previousRoot = current;

    // Applied takes the yaw out of the pose because it is about to go onto the GameObject;
    // InPlace keeps it, since a discarded yaw is deleted animation rather than "not
    // travelling" -- a turning clip would face one way forever. Mixamo draws the same line.
    anim::StripRootMotion(track.pose, track.binding.rootBone,
                          skeleton->skeleton.bindLocals[track.binding.rootBone],
                          track.mode == RootMotionMode::Applied);

    // Only Applied reports travel. Load-bearing now that ApplyRootMotion has no mode gate:
    // the per-track mode is the only thing deciding whether an object moves.
    if (track.mode != RootMotionMode::Applied)
        return {};

    return delta;
}

// ---------------------------------------------------------------------------
// Editor pose preview
// ---------------------------------------------------------------------------

void CAnimator::SetPreview(const ResourceAnimation* clip, const float time)
{
    m_previewClip = clip;
    m_previewTime = time;
}

void CAnimator::ApplyPreview()
{
    // CONSUMED, not latched -- which is what makes "closing the window stops the preview"
    // true by construction. A closed window is not called at all, so one that latched this
    // could never take it back.
    const ResourceAnimation* const previewClip = m_previewClip;
    m_previewClip = nullptr;

    if (!previewClip || !skeleton) return;

    // ENTIRELY LOCAL -- its own binding, instance and pose. It used to borrow m_from, on the
    // reasoning that OnUpdate re-derives that track's clip anyway. It does, but only the
    // POINTER: boundClip kept the preview clip's uid, so the next frame's compare rebound
    // the track and SetClip reset its time. Previewing any clip other than the playing one
    // therefore restarted it from zero every frame, re-firing every event near t = 0.
    const uint32_t clipUID = UIDOf(previewClip);

    const anim::AnimationBinding binding = anim::CreateBinding(
        previewClip->clip, clipUID, skeleton->skeleton, m_boundSkeleton);

    anim::AnimInstance instance;
    instance.SetClip(&previewClip->clip, clipUID, &binding);
    instance.Seek(m_previewTime);   // resets the cursor; a scrub jumps freely

    anim::Pose previewPose;
    anim::Sample(instance, skeleton->skeleton, m_boundSkeleton, previewPose);

    // No events and no root motion here (see SetPreview). m_blended is left alone too: its
    // other reader is EnterState's fold, which must capture the pose the GRAPH produced.
    if (!anim::BuildGlobals(skeleton->skeleton, previewPose, m_globals) ||
        !anim::BuildPalette(skeleton->skeleton, m_globals, m_palette))
    {
        m_palette.clear();
    }
}

// ---------------------------------------------------------------------------
// Animation events
// ---------------------------------------------------------------------------

void CAnimator::FireTrackEvents(const ClipTrack& track, const float timeBefore,
                                const bool wrapped)
{
    if (!track.clip || track.clip->events.empty()) return;

    // Derived, never passed in, so no call site can supply a flag that disagrees with the
    // instance it just advanced. `finished` is what closes the interval on a non-looping
    // clip's last frame, making an event authored at the very end reachable at all.
    const bool reversed = track.instance.speed < 0.0f;
    const bool finished = anim::IsFinished(track.instance);

    std::vector<int> fired;
    anim::CollectFiredEvents(track.clip->events, timeBefore, track.instance.time,
                             track.clip->clip.duration, wrapped, reversed, finished,
                             fired);

    if (fired.empty()) return;

    // Silent rather than warned: no CScript is the normal state while authoring, and a
    // warning here would fire on every footstep of an unscripted test scene.
    GameObject go = GetGameObject();
    CScript* scripts = go.IsValid() ? go.TryGetComponent<CScript>() : nullptr;
    if (!scripts) return;

    for (const int index : fired)
    {
        const anim::AnimationEvent& event = track.clip->events[index];
        scripts->DispatchAnimationEvent(event.name.c_str(), event.floatParam,
                                        event.stringParam.c_str());
    }
}

void CAnimator::ApplyRootMotion()
{
    if (m_rootDelta.translation == glm::vec3(0.0f) && m_rootDelta.yaw == 0.0f) return;

    GameObject go = GetGameObject();
    CTransform* transform = go.IsValid() ? go.TryGetComponent<CTransform>() : nullptr;

    if (!transform)
    {
        if (!m_warnedNoTransform)
        {
            m_warnedNoTransform = true;
            NOUS_WARN("[CAnimator] Root motion is Applied but the GameObject has no CTransform");
        }
        return;
    }

    // The delta is in the animation's space: rotate it into the object's and scale it, or a
    // turned character walks sideways and a scaled one footskates. Through the setters, not
    // the raw fields -- they are what mark the transform dirty for UpdateWorldMatrices.
    transform->Translate(transform->orientation * (m_rootDelta.translation * transform->scale));

    if (m_rootDelta.yaw != 0.0f)
    {
        const glm::quat yaw = glm::angleAxis(m_rootDelta.yaw, glm::vec3(0.0f, 1.0f, 0.0f));
        transform->SetOrientation(glm::normalize(transform->orientation * yaw));
    }
}

// ---------------------------------------------------------------------------
// Per-frame
// ---------------------------------------------------------------------------

void CAnimator::OnUpdate(const float deltaTime)
{
    const uint32_t controllerUID = UIDOf(controller);
    if (controllerUID != m_boundController)
    {
        m_boundController = controllerUID;
        m_boundGeneration = controller ? controller->generation : 0;

        // A new graph invalidates any in-flight transition: the frozen outgoing pose belongs
        // to a state that may not exist in this graph at all.
        m_fadeElapsed  = 0.0f;
        m_fadeDuration = 0.0f;
        m_to.clip      = nullptr;
        m_from.frozen  = false;

        m_currentState = controller ? controller->graph.defaultState : -1;

        // defaultState may not resolve -- renamed, deleted, or never named. Falling back to
        // the first state keeps the character animating and lets the editor report the
        // problem, rather than presenting a bind-pose statue that looks like a broken rig.
        if (controller && !controller->graph.IsValidState(m_currentState)
                       && !controller->graph.states.empty())
            m_currentState = 0;

        m_currentStateName.assign(GetCurrentStateName());

        m_from.clip       = ClipForState(m_currentState);
        m_from.stateIndex = m_currentState;
        m_from.mode       = ResolveRootMotion(m_currentState);
        RebindTrack(m_from);
        RebindTrack(m_to);

        SeedDeclaredParameters();
    }

    // A generation bump means the asset was RE-SAVED under a live animator. Not folded into
    // the block above: the controller UID is identical, so nothing there would notice.
    if (controller && controller->generation != m_boundGeneration)
    {
        m_boundGeneration = controller->generation;

        // Captured before the re-enter below throws it away. The restart was never chosen --
        // it falls out of EnterState -> RebindTrack -> SetClip, which resets `time` -- and
        // tuning a transition is only meaningful while the clip plays.
        const uint32_t heldClip = m_from.boundClip;
        const float    heldTime = m_from.instance.time;

        // By NAME, from the name remembered at enter: a re-save replaces the graph in place,
        // so reading states[m_currentState].name now would report whatever state has taken
        // that index and reordering two states would swap the character between them.
        int restored = m_currentStateName.empty()
                     ? -1
                     : controller->graph.FindState(m_currentStateName);

        // Same two-step fallback the bind path uses.
        if (!controller->graph.IsValidState(restored))
            restored = controller->graph.defaultState;
        if (!controller->graph.IsValidState(restored) && !controller->graph.states.empty())
            restored = 0;

        // An in-flight transition is cancelled rather than carried across: preserving it
        // would mean reconciling two graphs' transition identities for one frame of visual
        // continuity during an editor action.
        m_fadeElapsed  = 0.0f;
        m_fadeDuration = 0.0f;
        m_to.clip      = nullptr;
        m_from.frozen  = false;
        RebindTrack(m_to);

        EnterState(restored, 0.0f);

        if (heldClip != 0 && m_from.boundClip == heldClip)
            SeekTrackTo(m_from, heldTime);

        SeedDeclaredParameters();
    }

    // With no transition in flight, m_from IS the current state's clip -- re-derived every
    // frame rather than remembered, which self-heals every path that can change what a state
    // points at (the editor rebinding a clip, a clip that resolved late, a cleared slot).
    // None of those change the controller's UID, so nothing else here would notice them.
    //
    // Only when NOT fading: mid-fade m_from is the outgoing pose while m_currentState is
    // already the destination, so re-deriving would overwrite what the fade blends away from.
    if (m_fadeDuration <= 0.0f)
    {
        m_from.clip       = ClipForState(m_currentState);
        m_from.stateIndex = m_currentState;

        // Re-resolved here as well as at enter, which does not contradict the mode being
        // fixed per track: the guard above means m_from IS the current state, so there is no
        // second track to disagree with. It is what makes an edit to either mode reach a
        // character already standing in that state.
        m_from.mode = ResolveRootMotion(m_currentState);
    }

    // Rebind on a UID mismatch rather than on an explicit call: integer compares covering
    // every path that can change a slot, with no "remember to call Bind()" contract.
    const uint32_t skeletonUID = UIDOf(skeleton);
    if (skeletonUID != m_boundSkeleton)
    {
        m_boundSkeleton = skeletonUID;

        // Any mismatch reported against the previous skeleton is stale. Without this,
        // correcting one wrong .nskel then dropping a second would warn about neither.
        warnedSkeletonMismatch = false;

        // Cancel any fade: a frozen source pose belongs to the OLD skeleton and would fail
        // ArePosesCompatible, so this makes the Blend guard unreachable by construction.
        m_fadeElapsed  = 0.0f;
        m_fadeDuration = 0.0f;
        m_to.clip      = nullptr;
        m_from.frozen  = false;

        RebindTrack(m_from);
        RebindTrack(m_to);
    }

    if (UIDOf(m_from.clip) != m_from.boundClip) RebindTrack(m_from);
    if (UIDOf(m_to.clip)   != m_to.boundClip)   RebindTrack(m_to);

    // Evaluated BEFORE the clocks advance, so it reads the progress the previous frame left:
    // firing against an unsampled pose would let a transition leave a state that never
    // rendered. Ahead of the !IsBound() return on purpose -- a state whose clip did not
    // resolve leaves the animator unbound, and skipping evaluation would strand it there.
    if (controller && !m_graphSuppressedThisFrame)
    {
        const anim::TransitionResult result = anim::EvaluateController(
            controller->graph, m_currentState, GraphProgress(), parameters);

        if (result.fired)
            EnterState(result.toState, result.duration);
    }

    // Cleared at the END, which is what makes a CrossFade's override last exactly one frame.
    // Before the early return, so an unbound animator cannot leave the flag stuck set.
    m_graphSuppressedThisFrame = false;

    if (!IsBound())
    {
        m_globals.clear();
        m_palette.clear();
        m_rootDelta = {};
        ApplyPreview();   // a clip can be previewed before the graph binds one
        return;
    }

    // Per frame rather than in RebindTrack: nothing about the slot changes when the user
    // ticks the Inspector checkbox, so a rebind-only seed would never see the edit.
    SeedPlaybackSettings(m_from);
    SeedPlaybackSettings(m_to);

    // LOAD-BEARING, and for BOTH tracks. instance.binding points at a member of THIS object,
    // and EnTT relocates components by memcpy when a pool grows -- so a pointer stored once
    // survives the move as a dangling read. Missing one track reproduces the bug on the
    // interrupted-transition path only, which reads as "transitions break once the scene
    // gets big enough" rather than as a pointer bug.
    m_from.instance.binding = &m_from.binding;
    m_to.instance.binding   = &m_to.binding;

    anim::RootMotionDelta deltaFrom;
    anim::RootMotionDelta deltaTo;

    // Both advancing tracks fire their events, outgoing included: an attack interrupted at
    // 90% has visually landed its hit. A frozen track advances nothing and so fires nothing,
    // which falls out of this branch rather than being a rule. Accepted cost: a footstep can
    // double up mid-blend when both clips carry one at a similar phase.
    if (!m_from.frozen)
    {
        const float timeBefore = m_from.instance.time;
        const bool  wrapped    = anim::Advance(m_from.instance, deltaTime);
        anim::Sample(m_from.instance, skeleton->skeleton, m_boundSkeleton, m_from.pose);
        deltaFrom = ExtractTrackRootMotion(m_from, wrapped);
        FireTrackEvents(m_from, timeBefore, wrapped);
    }

    if (m_fadeDuration > 0.0f && m_to.boundClip != 0)
    {
        const float timeBefore = m_to.instance.time;
        const bool  wrapped    = anim::Advance(m_to.instance, deltaTime);
        anim::Sample(m_to.instance, skeleton->skeleton, m_boundSkeleton, m_to.pose);
        deltaTo = ExtractTrackRootMotion(m_to, wrapped);
        FireTrackEvents(m_to, timeBefore, wrapped);

        m_fadeElapsed += deltaTime;
        const float weight = glm::clamp(m_fadeElapsed / m_fadeDuration, 0.0f, 1.0f);

        // Blend the DELTAS, not the blended pose: the root position sweeping from one clip's
        // hips to the other's is an artifact of blending, not motion, and a positional delta
        // cannot tell the difference.
        m_rootDelta = anim::BlendRootDelta(deltaFrom, deltaTo, weight);

        bool blendFailed = false;
        if (!anim::Blend(m_from.pose, m_to.pose, weight, m_blended))
        {
            // Reachable only on mismatched skeleton UIDs or bone counts, which the
            // skeleton-swap cancel above makes unreachable in practice. Snapping rather than
            // asserting: an assert here would kill a shipped game over something recoverable.
            m_blended   = m_to.pose;
            blendFailed = true;
        }

        if (weight >= 1.0f || blendFailed)
        {
            // Blend is bit-exact at weight 1, so promoting introduces no pop. The move
            // invalidates m_from.instance.binding, which the per-frame re-point repairs.
            m_from = std::move(m_to);
            m_to   = ClipTrack{};
            m_fadeElapsed  = 0.0f;
            m_fadeDuration = 0.0f;
        }
    }
    else
    {
        m_blended   = m_from.pose;
        m_rootDelta = deltaFrom;
    }

    // NO mode gate here, deliberately: ExtractTrackRootMotion already zeroes every track that
    // is not Applied. Gating on the component's mode would make a per-state Applied
    // unreachable; gating on m_currentState would snap travel off on a transition's first
    // frame, since the current state is the destination from that instant.
    ApplyRootMotion();

    // Guarded: on failure the globals are stale, and a palette built from them would deform
    // the mesh to a pose that was never sampled. Clearing is what makes the renderer skip
    // this animator instead.
    if (!anim::BuildGlobals(skeleton->skeleton, m_blended, m_globals) ||
        !anim::BuildPalette(skeleton->skeleton, m_globals, m_palette))
    {
        m_palette.clear();
    }

    // LAST, so it overrides the frame's real pose rather than being overwritten by it.
    ApplyPreview();
}

// ---------------------------------------------------------------------------
// Serialization
//
// assetPath + libraryPath + UID per slot -- the CMesh / CAudioSource shape, which is what
// lets GameApp resolve both resources from a Library/ that ships without Assets/.
// ---------------------------------------------------------------------------

JsonObject CAnimator::Serialize() const
{
    JsonObject root;
    root.Set("type", GetType());

    root.Set("skeletonAssetPath", skeleton ? skeleton->GetAssetsPath() : "");
    if (skeleton)
    {
        root.Set("skeletonLibraryPath", skeleton->GetLibraryPath());
        root.Set("skeletonUID",         static_cast<double>(skeleton->GetUID()));
    }

    root.Set("controllerAssetPath", controller ? controller->GetAssetsPath() : "");
    if (controller)
    {
        root.Set("controllerLibraryPath", controller->GetLibraryPath());
        root.Set("controllerUID",         static_cast<double>(controller->GetUID()));
    }

    // No "speed"/"loop": they live on ResourceAnimation::settings now.
    root.Set("fadeSeconds",     fadeSeconds);
    root.Set("speedMultiplier", speedMultiplier);
    root.Set("rootMotion",  RootMotionToString(rootMotion));
    return root;
}

void CAnimator::OnDestroy()
{
    IResourceLoader* rm = Services().resources;
    if (!rm)
        return;   // headless scene -- nothing was ever acquired

    if (skeleton && skeleton->IsLoaded())
        rm->UnloadResource(skeleton->GetUID());

    // The controller only. Its own clip references are released by
    // ImporterAnimationController::Evict -- this component never acquired them.
    if (controller && controller->IsLoaded())
        rm->UnloadResource(controller->GetUID());
}

void CAnimator::Deserialize(const JsonObject& obj)
{
    fadeSeconds     = obj.GetFloat("fadeSeconds",     fadeSeconds);
    speedMultiplier = obj.GetFloat("speedMultiplier", speedMultiplier);
    rootMotion  = RootMotionFromString(obj.GetString("rootMotion", "Baked"));

    IResourceLoader* rm = Services().resources;
    if (!rm)
        return;   // headless scene -- slots stay null and OnUpdate no-ops

    // GAME path first (straight from Library, no .meta read), then the EDITOR path.
    const auto resolve = [rm](const std::string& assetPath,
                              const std::string& libraryPath,
                              const uint32_t     uid,
                              const ResourceType type) -> ResourceBase*
    {
        if (assetPath.empty() && libraryPath.empty())
            return nullptr;

        if (!libraryPath.empty() && uid != 0)
        {
            if (ResourceBase* r = rm->CreateResourceFromLibrary(
                    uid, type, nous::engine::filesystem::GetFilename(assetPath),
                    assetPath, libraryPath))
                return r;
        }

        return assetPath.empty() ? nullptr : rm->CreateResource(assetPath);
    };

    if (ResourceBase* r = resolve(obj.GetString("skeletonAssetPath"),
                                  obj.GetString("skeletonLibraryPath"),
                                  static_cast<uint32_t>(obj.GetDouble("skeletonUID", 0.0)),
                                  ResourceType::SKELETON))
        skeleton = down_cast<ResourceSkeleton*>(r);

    if (ResourceBase* r = resolve(obj.GetString("controllerAssetPath"),
                                  obj.GetString("controllerLibraryPath"),
                                  static_cast<uint32_t>(obj.GetDouble("controllerUID", 0.0)),
                                  ResourceType::ANIMATION_CONTROLLER))
        controller = down_cast<ResourceAnimationController*>(r);

    // The old per-animator "clips" array and "clipAssetPath" are gone rather than migrated:
    // there is no longer a per-animator clip list to carry them into. An animator in an
    // older scene loads with no controller and plays nothing until one is assigned.
}
