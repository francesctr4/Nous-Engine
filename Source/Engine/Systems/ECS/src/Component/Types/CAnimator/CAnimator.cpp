#include <ECS/Component/Types/CAnimator/CAnimator.h>

#include <AnimationSystem/Blending.h>
#include <AnimationSystem/Palette.h>
#include <AnimationSystem/Sampling.h>
#include <AnimationSystem/RootMotion.h>
#include <EngineCore/Casts.h>
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

    // Serialized as a STRING, like CLight's and CAudioSource's enums: the numeric
    // value would silently change meaning if a mode were ever inserted mid-enum.
    const char* RootMotionToString(const RootMotionMode m)
    {
        switch (m)
        {
            case RootMotionMode::Applied: return "Applied";
            case RootMotionMode::InPlace: return "InPlace";
            case RootMotionMode::Baked:   // fallthrough
            default:                      return "Baked";
        }
    }

    RootMotionMode RootMotionFromString(const std::string& s)
    {
        if (s == "Applied") return RootMotionMode::Applied;
        if (s == "InPlace") return RootMotionMode::InPlace;
        return RootMotionMode::Baked;   // also the pre-root-motion scene case
    }
}

// ---------------------------------------------------------------------------
// Binding
// ---------------------------------------------------------------------------

const ResourceAnimation* CAnimator::CurrentClip() const { return m_from.clip; }

float CAnimator::GetNormalizedTime() const
{
    // Follows m_from -- the same track CurrentClip() reports -- so the two can never
    // describe different clips.
    if (!m_from.clip || m_from.boundClip == 0)
        return 0.0f;

    const float duration = m_from.clip->clip.duration;
    if (duration <= 0.0f)
        return 0.0f;   // a zero-duration clip has no meaningful progress

    return m_from.instance.time / duration;
}

ResourceAnimation* CAnimator::ClipForState(const int stateIndex) const
{
    if (!controller || !controller->graph.IsValidState(stateIndex))
        return nullptr;

    // clipIndex, never the state index: the two coincide today because the importer
    // fills one clip slot per state, but clipIndex is what the pure layer carries
    // and it is -1 for any state whose clip did not resolve.
    const int clipIndex = controller->graph.states[stateIndex].clipIndex;
    if (clipIndex < 0 || static_cast<size_t>(clipIndex) >= controller->clips.size())
        return nullptr;

    return controller->clips[clipIndex];
}

void CAnimator::EnterState(const int stateIndex, const float fadeSeconds)
{
    // The destination becomes current IMMEDIATELY -- design §4. There is only ever
    // one current state; the outgoing side is a pose, not a state. That is what
    // makes exit time and conditions measure the INCOMING clip from the frame it is
    // entered, which is the only reading under which "when the attack finishes"
    // means the attack.
    m_currentState = stateIndex;

    ResourceAnimation* target = ClipForState(stateIndex);

    if (fadeSeconds <= 0.0f)
    {
        m_from.clip = target;
        RebindTrack(m_from);
        m_to.clip = nullptr;
        RebindTrack(m_to);
        m_fadeElapsed  = 0.0f;
        m_fadeDuration = 0.0f;
        return;
    }

    // A fade is already running: fold the CURRENT blended pose into the outgoing
    // track and fade from there. That keeps the animator at exactly two tracks under
    // arbitrary re-triggering, and it is why ClipTrack has `frozen` at all -- a
    // frozen track is a pose with no clip advancing behind it. The two-track ceiling
    // IS the interruption model, deliberately, not a limitation being worked around.
    //
    // RebindTrack clears `frozen`, so the capture must happen BEFORE rebinding the
    // target -- and m_from must never be rebound on this path, since that would
    // resample it and throw the captured pose away.
    if (m_fadeDuration > 0.0f)
    {
        m_from.pose   = m_blended;
        m_from.frozen = true;
    }

    m_to.clip = target;
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

    track.instance.loop  = track.clip->settings.loop;
    track.instance.speed = track.clip->settings.speed * speedMultiplier;
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

    // Preallocate here rather than resizing per character per frame. Sample()
    // would size the pose itself, but only on its first call.
    track.pose.skeleton = m_boundSkeleton;
    track.pose.bones.assign(skeleton->skeleton.BoneCount(), anim::Transform{});

    // The root's transform at t=0 and t=duration, sampled once here so the
    // loop-seam split costs nothing per frame. Sampling the whole skeleton twice
    // is wasteful in the abstract and free in practice -- binding is rare, and
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

        // The instance is at t=0 after a rebind, so the first frame's delta is
        // measured from the clip's start rather than from a stale pose.
        track.previousRoot = track.rootAtStart;
    }
}

// ---------------------------------------------------------------------------
// Root motion
// ---------------------------------------------------------------------------

anim::RootMotionDelta CAnimator::ExtractTrackRootMotion(ClipTrack& track, const bool wrapped)
{
    if (rootMotion == RootMotionMode::Baked) return {};
    if (track.frozen || track.binding.rootBone < 0) return {};
    if (static_cast<size_t>(track.binding.rootBone) >= track.pose.bones.size()) return {};

    // BY VALUE, not by reference: StripRootMotion mutates this very bone, so a
    // reference would be read back already stripped and every frame after the
    // first would measure zero travel.
    const anim::Transform current = track.pose.bones[track.binding.rootBone];

    const anim::RootMotionDelta delta = anim::ComputeRootDelta(
        track.previousRoot, current, track.rootAtStart, track.rootAtEnd, wrapped);

    track.previousRoot = current;

    // Applied takes the yaw out of the pose because it is about to go onto the
    // GameObject; InPlace keeps it, because a discarded yaw is not "not
    // travelling" but deleted animation -- a turning clip would face one way
    // forever. Same line Mixamo's own In Place export draws.
    anim::StripRootMotion(track.pose, track.binding.rootBone,
                          skeleton->skeleton.bindLocals[track.binding.rootBone],
                          rootMotion == RootMotionMode::Applied);

    return delta;
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

    // The delta is in the animation's space: rotate it into the object's and scale
    // it, or a character that is turned walks sideways and a scaled one footskates.
    // Through the setters, never the raw fields -- they are what mark the transform
    // dirty for UpdateWorldMatrices.
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
    // The CONTROLLER is now the source of truth for what may play -- the authored
    // clip list it replaced is gone, and with it the per-frame reseeding that
    // policed it. A clip leaving the graph is handled one level up: the controller
    // releases it, and the bind below repoints both tracks.
    const uint32_t controllerUID = UIDOf(controller);
    if (controllerUID != m_boundController)
    {
        m_boundController = controllerUID;
        m_boundGeneration = controller ? controller->generation : 0;

        // A new graph invalidates any in-flight transition: the outgoing frozen pose
        // belongs to a state that may not exist in this graph at all.
        m_fadeElapsed  = 0.0f;
        m_fadeDuration = 0.0f;
        m_to.clip      = nullptr;
        m_from.frozen  = false;

        m_currentState = controller ? controller->graph.defaultState : -1;

        // defaultState may not resolve -- a renamed or deleted state, or an asset
        // that never named one. Falling back to the first state keeps the character
        // animating and lets the editor report the problem, rather than presenting a
        // bind-pose statue that looks like a broken rig.
        if (controller && !controller->graph.IsValidState(m_currentState)
                       && !controller->graph.states.empty())
            m_currentState = 0;

        m_from.clip = ClipForState(m_currentState);
        RebindTrack(m_from);
        RebindTrack(m_to);
    }

    // When no transition is in flight, m_from IS the current state's clip -- RE-DERIVED
    // every frame rather than remembered. This is what replaces the authored-list
    // reseeding the clip vector needed, and it self-heals every path that can change
    // what a state points at: the editor rebinding a state's clip, a clip that
    // resolved after the controller did, a state whose clip was cleared. None of
    // those change the controller's UID, so nothing else here would notice them.
    //
    // Only when NOT fading: mid-fade m_from is the OUTGOING pose and m_currentState
    // is already the destination (design §4), so re-deriving there would overwrite
    // the very thing the fade is blending away from.
    if (m_fadeDuration <= 0.0f)
        m_from.clip = ClipForState(m_currentState);

    // Rebind on a UID mismatch rather than on an explicit call. Integer comparisons,
    // and they cover every path that can change a slot -- Inspector drop, Inspector
    // clear, Deserialize, a resource going away -- with no "remember to call Bind()"
    // contract for a future call site to forget.
    const uint32_t skeletonUID = UIDOf(skeleton);
    if (skeletonUID != m_boundSkeleton)
    {
        m_boundSkeleton = skeletonUID;

        // The rig changed, so any mesh/rig mismatch reported against the previous
        // skeleton is stale. Without this, correcting one wrong .nskel and then
        // dropping a second wrong one would warn about neither.
        warnedSkeletonMismatch = false;

        // Cancel any fade: a frozen source pose belongs to the OLD skeleton and would
        // fail ArePosesCompatible against a freshly sized target. Cancelling makes
        // that unreachable by construction instead of leaning on the Blend guard.
        m_fadeElapsed  = 0.0f;
        m_fadeDuration = 0.0f;
        m_to.clip      = nullptr;
        m_from.frozen  = false;

        RebindTrack(m_from);
        RebindTrack(m_to);
    }

    if (UIDOf(m_from.clip) != m_from.boundClip) RebindTrack(m_from);
    if (UIDOf(m_to.clip)   != m_to.boundClip)   RebindTrack(m_to);

    if (!IsBound())
    {
        m_globals.clear();
        m_palette.clear();
        m_rootDelta = {};
        return;
    }

    // Each track takes loop/speed from ITS OWN clip, which is the whole point of the
    // settings living on the resource: during a cross-fade the two tracks routinely
    // disagree. Pushed every frame rather than seeded in RebindTrack so an Inspector
    // edit reaches a clip that is already playing -- nothing about the slot changes
    // when the user ticks the checkbox, so RebindTrack would never run.
    SeedPlaybackSettings(m_from);
    SeedPlaybackSettings(m_to);

    // LOAD-BEARING, and reassigned EVERY frame for BOTH tracks rather than once in
    // RebindTrack.
    //
    // instance.binding points at a member of THIS object, and EnTT relocates
    // components by memcpy when a pool grows -- so a pointer stored once survives the
    // move as a dangling read into vacated memory. One assignment per frame makes the
    // self-reference self-healing at no meaningful cost. Missing ONE track reproduces
    // the bug on the interrupted-transition path only, which would look like
    // "transitions break once the scene gets big enough" rather than a pointer bug.
    m_from.instance.binding = &m_from.binding;
    m_to.instance.binding   = &m_to.binding;

    anim::RootMotionDelta deltaFrom;
    anim::RootMotionDelta deltaTo;

    // A frozen track holds a captured blend; advancing or sampling it would replace
    // that pose with the clip's own, which is exactly what the capture avoided.
    if (!m_from.frozen)
    {
        const bool wrapped = anim::Advance(m_from.instance, deltaTime);
        anim::Sample(m_from.instance, skeleton->skeleton, m_boundSkeleton, m_from.pose);
        deltaFrom = ExtractTrackRootMotion(m_from, wrapped);
    }

    if (m_fadeDuration > 0.0f && m_to.boundClip != 0)
    {
        const bool wrapped = anim::Advance(m_to.instance, deltaTime);
        anim::Sample(m_to.instance, skeleton->skeleton, m_boundSkeleton, m_to.pose);
        deltaTo = ExtractTrackRootMotion(m_to, wrapped);

        m_fadeElapsed += deltaTime;
        const float weight = glm::clamp(m_fadeElapsed / m_fadeDuration, 0.0f, 1.0f);

        // Both poses are already stripped, so the blended pose carries no travel --
        // which also removes the hip-slide the position sweep would otherwise put
        // into the pose itself.
        m_rootDelta = anim::BlendRootDelta(deltaFrom, deltaTo, weight);

        bool blendFailed = false;
        if (!anim::Blend(m_from.pose, m_to.pose, weight, m_blended))
        {
            // Reachable only with mismatched skeleton UIDs or bone counts, which the
            // skeleton-swap cancel makes unreachable in practice; it stays as defence
            // because Blend is [[nodiscard]] and the result must be consumed anyway.
            // Snapping rather than asserting: an assert here would kill a shipped
            // game over something recoverable.
            m_blended   = m_to.pose;
            blendFailed = true;
        }

        if (weight >= 1.0f || blendFailed)
        {
            // Blend is bit-exact at weight 1, so promoting introduces no pop. The move
            // takes the pose and cursor vectors rather than copying them; it also
            // invalidates m_from.instance.binding, which the next frame's
            // unconditional re-point above repairs -- that is why it is per-frame.
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

    if (rootMotion == RootMotionMode::Applied)
        ApplyRootMotion();

    // Guarded rather than fire-and-forget: on failure the globals are stale, and a
    // palette built from them would deform the mesh to a pose that was never
    // sampled. Clearing is what makes the renderer skip this animator instead --
    // GetPalette().empty() is its skinned-geometry test.
    //
    // rootGlobalInverse is left at its identity default: `offsets` and `globals` are
    // built in the same node space, so globals[b] * offsets[b] already maps mesh
    // space to animated model space. See the note on BuildPalette in Palette.h.
    if (!anim::BuildGlobals(skeleton->skeleton, m_blended, m_globals) ||
        !anim::BuildPalette(skeleton->skeleton, m_globals, m_palette))
    {
        m_palette.clear();
    }
}

// ---------------------------------------------------------------------------
// Serialization
//
// assetPath + libraryPath + UID per slot -- the CMesh / CAudioSource shape, which
// is what lets GameApp resolve both resources with no .meta files, from a
// Library/ that ships without Assets/.
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

    // One slot, in the same three-field shape as the skeleton. The clip list that
    // used to live here belongs to the controller's states now, so a scene carries
    // no clip references of its own at all.
    root.Set("controllerAssetPath", controller ? controller->GetAssetsPath() : "");
    if (controller)
    {
        root.Set("controllerLibraryPath", controller->GetLibraryPath());
        root.Set("controllerUID",         static_cast<double>(controller->GetUID()));
    }

    // No "speed"/"loop" here -- they moved to ResourceAnimation::settings, which the
    // .nanim stub and the library binary carry. A scene that predates the move loses
    // whatever it had set; scenes are authored data, but a per-animator value has no
    // per-clip destination to migrate INTO.
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

    // The controller only. Its OWN clip references are released by
    // ImporterAnimationController::Evict when the controller itself evicts -- this
    // component never acquired them and must not give back what it does not hold.
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

    // GAME path first (straight from Library, no .meta read), then the EDITOR
    // path / fallback via the asset path.
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

    // MVP-E's "clips" array and the pre-MVP-E "clipAssetPath" fallback are both GONE
    // rather than migrated. That compatibility path existed to carry a per-animator
    // clip list forward, and there is no longer any such list to carry it into: the
    // clips belong to a controller asset that the scene cannot invent. An animator in
    // an older scene loads with no controller and plays nothing until one is assigned,
    // which is the honest outcome and is visible immediately.
}
