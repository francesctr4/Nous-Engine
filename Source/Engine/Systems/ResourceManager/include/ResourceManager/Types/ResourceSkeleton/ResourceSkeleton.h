#pragma once

#include <AnimationSystem/Skeleton/Skeleton.h>
#include <EngineCore/EngineExport.h>
#include <ResourceManager/Core/ResourceBase.h>

#include <cstdint>

// A rig: bone names, parents, bind-pose offsets and bind-pose locals.
//
// SkeletonData is composed BY VALUE rather than having its fields redeclared here: the
// animation library owns the shape and knows nothing about resources, while this class
// adds a UID, a path and a ref-count on top.
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
    // ADVISORY ONLY: it would be load-bearing if two importers derived bone orderings
    // independently, but one ParseModel produces the mesh's boneIDs and this skeleton
    // together, so they cannot disagree. A diagnostic, not a correctness mechanism.
    uint64_t nameHash = 0;
};
