#pragma once

#include <AnimationSystem/AnimClip.h>
#include <AnimationSystem/Skeleton.h>
#include <ResourceManager/Types/ResourceMesh/ImporterMesh.h>

#include <string>
#include <vector>

namespace nous::engine::resource_manager
{
    // Everything one model file contains, parsed ONCE, in engine types.
    //
    // This replaces the spec's AssimpSceneCache outright. That class existed only
    // because three importers each wanted to parse the same FBX; with a single
    // pre-pass there is nothing to cache -- no refcount, no mutex, and no "the load
    // flags must be part of the cache key" hazard (the spec flags that one
    // specifically for aiProcess_LimitBoneWeights).
    //
    // It also dissolves the spec's "canonical bone ordering -- critical" section.
    // The worry there is that ImporterMesh derives one bone order (only bones
    // carrying weights) while ImporterSkeleton derives another (every joint), and
    // the fix proposed is stamping a bone-name hash into both binaries to detect
    // the disagreement. One parse producing both the skeleton array AND the mesh's
    // boneIDs cannot disagree by construction. A hash stays worth having as a cheap
    // load-time check; it is no longer load-bearing.
    struct ModelImportData
    {
        std::vector<SubMeshData> submeshes;

        // Indexed by assimp material index; entries are Assets/-relative .nmat
        // paths. Empty when the format's material slots are not trusted -- see
        // ModelParser.cpp for the glTF-only gate this inherits from ImporterMesh.
        std::vector<std::string> materialPaths;

        // Empty when the model has no bones at all. `clips` can be non-empty with
        // an empty skeleton (a node-animated prop), which is why the two are
        // independent rather than one optional block.
        animation_system::SkeletonData              skeleton;
        std::vector<animation_system::AnimClipData> clips;

        [[nodiscard]] bool HasSkeleton() const { return skeleton.BoneCount() > 0; }
    };
}
