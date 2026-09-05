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

void CAnimator::Rebind()
{
    m_boundClip     = UIDOf(clip);
    m_boundSkeleton = UIDOf(skeleton);

    // A slot changed, so any mesh/rig mismatch reported against the previous skeleton
    // is stale. Without this, correcting one wrong .nskel and then dropping a second
    // wrong one would warn about neither.
    warnedSkeletonMismatch = false;

    if (!clip || !skeleton)
    {
        m_binding = {};
        m_pose    = {};
        m_globals.clear();
        m_palette.clear();
        m_instance.SetClip(nullptr, 0, nullptr);
        m_boundClip     = 0;
        m_boundSkeleton = 0;
        return;
    }

    m_binding = anim::CreateBinding(clip->clip, m_boundClip,
                                    skeleton->skeleton, m_boundSkeleton);

    m_instance.SetClip(&clip->clip, m_boundClip, &m_binding);

    // Preallocate here rather than resizing per character per frame. Sample()
    // would size the pose itself, but only on its first call -- and the globals
    // buffer it feeds has no such guarantee.
    m_pose.skeleton = m_boundSkeleton;
    m_pose.bones.assign(skeleton->skeleton.BoneCount(), anim::Transform{});
    m_globals.assign(skeleton->skeleton.BoneCount(), glm::mat4(1.0f));
    m_palette.assign(skeleton->skeleton.BoneCount(), glm::mat4(1.0f));
}

// ---------------------------------------------------------------------------
// Per-frame
// ---------------------------------------------------------------------------

void CAnimator::OnUpdate(const float deltaTime)
{
    // Authoring fields are live, so an Inspector edit applies on the next frame.
    m_instance.speed = speed;
    m_instance.loop  = loop;

    // Rebind on a UID mismatch rather than on an explicit call. Two integer
    // comparisons, and it covers every path that can change a slot -- Inspector
    // drop, Inspector clear, Deserialize, a resource going away -- with no
    // "remember to call Bind()" contract for a future call site to forget.
    if (UIDOf(clip) != m_boundClip || UIDOf(skeleton) != m_boundSkeleton)
        Rebind();

    if (!IsBound())
        return;

    // LOAD-BEARING, and reassigned EVERY frame rather than once in Rebind().
    //
    // m_instance.binding points at m_binding, a member of THIS object, and EnTT
    // relocates components by memcpy when a pool grows -- so a pointer stored once
    // survives the move as a dangling read into vacated memory. One assignment per
    // frame makes the self-reference self-healing at no meaningful cost. Do not
    // "optimize" it back into Rebind(). Pinned by t_CAnimator.SurvivesPoolRelocation.
    m_instance.binding = &m_binding;

    anim::Advance(m_instance, deltaTime);
    anim::Sample(m_instance, skeleton->skeleton, m_boundSkeleton, m_pose);

    // Guarded rather than fire-and-forget: on failure the globals are stale, and a
    // palette built from them would deform the mesh to a pose that was never
    // sampled. Clearing is what makes the renderer skip this animator instead --
    // GetPalette().empty() is its skinned-geometry test.
    //
    // rootGlobalInverse is left at its identity default: `offsets` and `globals` are
    // built in the same node space, so globals[b] * offsets[b] already maps mesh
    // space to animated model space. See the note on BuildPalette in Palette.h.
    if (!anim::BuildGlobals(skeleton->skeleton, m_pose, m_globals) ||
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

    root.Set("clipAssetPath", clip ? clip->GetAssetsPath() : "");
    if (clip)
    {
        root.Set("clipLibraryPath", clip->GetLibraryPath());
        root.Set("clipUID",         static_cast<double>(clip->GetUID()));
    }

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

    if (clip && clip->IsLoaded())
        rm->UnloadResource(clip->GetUID());
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

    if (ResourceBase* r = resolve(obj.GetString("clipAssetPath"),
                                  obj.GetString("clipLibraryPath"),
                                  static_cast<uint32_t>(obj.GetDouble("clipUID", 0.0)),
                                  ResourceType::ANIMATION))
        clip = down_cast<ResourceAnimation*>(r);
}
