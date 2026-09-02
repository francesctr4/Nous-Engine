#pragma once

#include <AnimationSystem/Skeleton.h>
#include <EngineCore/EngineExport.h>
#include <ResourceManager/Core/ResourceBase.h>

#include <cstdint>

// A rig: bone names, parents, bind-pose offsets and bind-pose locals.
//
// SkeletonData is composed BY VALUE rather than having its fields redeclared here.
// The animation library owns the shape and knows nothing about resources; this
// class adds a UID, a path and a ref-count on top. Two declarations of the same
// fields would be two places to drift.
//
// NO GPU RESIDENCY. The bone palette is per-animator and rebuilt each frame, so
// ImporterSkeleton's Upload/Release are no-ops.
class ResourceSkeleton : public ResourceBase
{
public:
    NOUS_ENGINE_API explicit ResourceSkeleton(uint32_t uid);

    nous::engine::animation_system::SkeletonData skeleton;

    // FNV-1a over the joined bone names -- a cheap "is this the same rig?" check.
    //
    // ADVISORY ONLY. The spec made this hash load-bearing because it assumed two
    // importers would derive bone orderings independently and could disagree; with
    // one ParseModel producing the mesh's boneIDs and this skeleton together, they
    // cannot. It survives as a diagnostic, not as a correctness mechanism.
    uint64_t nameHash = 0;
};
