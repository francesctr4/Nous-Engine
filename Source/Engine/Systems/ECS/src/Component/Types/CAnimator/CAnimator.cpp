#include <ECS/Component/Types/CAnimator/CAnimator.h>

#include <AnimationSystem/Palette.h>
#include <AnimationSystem/Sampling.h>
#include <EngineCore/Casts.h>
#include <ECS/ComponentServices.h>
#include <FileSystem/FileSystem.h>   // GetFilename
#include <ResourceManager/Core/IResourceLoader.h>
#include <ResourceManager/Core/ResourceBase.h>
#include <ResourceManager/Types/ResourceAnimation/ResourceAnimation.h>
#include <ResourceManager/Types/ResourceSkeleton/ResourceSkeleton.h>
#include <ResourceManager/Types/ResourceType.h>
#include <Utils/Serialization/JsonArray.h>
#include <Utils/Serialization/JsonObject.h>

#include <string>

namespace anim = nous::engine::animation_system;

namespace
{
    uint32_t UIDOf(const ResourceBase* resource) { return resource ? resource->GetUID() : 0u; }
}

// ---------------------------------------------------------------------------
// Binding
// ---------------------------------------------------------------------------

const ResourceAnimation* CAnimator::CurrentClip() const { return m_from.clip; }

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
}

// ---------------------------------------------------------------------------
// Per-frame
// ---------------------------------------------------------------------------

void CAnimator::OnUpdate(const float deltaTime)
{
    // Task 3 of MVP-E replaces this with Play()-driven selection. For now the list
    // simply plays its first entry, so the two tracks change no behaviour.
    m_from.clip = clips.empty() ? nullptr : clips.front();

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

        RebindTrack(m_from);
        RebindTrack(m_to);
    }

    if (UIDOf(m_from.clip) != m_from.boundClip) RebindTrack(m_from);
    if (UIDOf(m_to.clip)   != m_to.boundClip)   RebindTrack(m_to);

    if (!IsBound())
    {
        m_globals.clear();
        m_palette.clear();
        return;
    }

    // Authoring fields are live, so an Inspector edit applies on the next frame.
    m_from.instance.speed = speed;
    m_from.instance.loop  = loop;
    m_to.instance.speed   = speed;
    m_to.instance.loop    = loop;

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

    anim::Advance(m_from.instance, deltaTime);
    anim::Sample(m_from.instance, skeleton->skeleton, m_boundSkeleton, m_from.pose);
    m_blended = m_from.pose;

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

    JsonArray clipArr;
    for (const ResourceAnimation* c : clips)
    {
        if (!c)
            continue;

        JsonObject entry;
        entry.Set("assetPath",   c->GetAssetsPath());
        entry.Set("libraryPath", c->GetLibraryPath());
        entry.Set("uid",         static_cast<double>(c->GetUID()));
        clipArr.Append(std::move(entry));
    }
    root.Set("clips", std::move(clipArr));

    root.Set("speed", speed);
    root.Set("loop",  loop);
    return root;
}

void CAnimator::OnDestroy()
{
    IResourceLoader* rm = Services().resources;
    if (!rm)
        return;   // headless scene -- nothing was ever acquired

    if (skeleton && skeleton->IsLoaded())
        rm->UnloadResource(skeleton->GetUID());

    // Every clip, not just the playing one: AddComponent fires OnDestroy on the
    // component it REPLACES, which PrefabManager does to a prefab root on every
    // migration -- so releasing one would leak a reference per extra clip per load.
    for (const ResourceAnimation* c : clips)
        if (c && c->IsLoaded())
            rm->UnloadResource(c->GetUID());
}

void CAnimator::Deserialize(const JsonObject& obj)
{
    speed = obj.GetFloat("speed", speed);
    loop  = obj.GetBool ("loop",  loop);

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

    clips.clear();

    JsonArray clipArr = obj.GetArray("clips");
    for (int i = 0; i < clipArr.Count(); ++i)
    {
        const JsonObject entry = clipArr.GetObject(i);
        if (ResourceBase* r = resolve(entry.GetString("assetPath"),
                                      entry.GetString("libraryPath"),
                                      static_cast<uint32_t>(entry.GetDouble("uid", 0.0)),
                                      ResourceType::ANIMATION))
            clips.push_back(down_cast<ResourceAnimation*>(r));
    }

    // LEGACY, one deliberate compatibility path: a scene saved before MVP-E carries a
    // single "clipAssetPath". The no-versioning rule is about Library/, a derived
    // cache that regenerates; scenes are authored data that does not, and silently
    // emptying every animator in them is not an acceptable cost. Delete once the
    // project's scenes have been re-saved.
    if (clips.empty())
    {
        if (ResourceBase* r = resolve(obj.GetString("clipAssetPath"),
                                      obj.GetString("clipLibraryPath"),
                                      static_cast<uint32_t>(obj.GetDouble("clipUID", 0.0)),
                                      ResourceType::ANIMATION))
            clips.push_back(down_cast<ResourceAnimation*>(r));
    }
}
