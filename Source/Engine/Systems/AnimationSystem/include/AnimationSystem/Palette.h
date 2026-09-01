#pragma once

#include <AnimationSystem/Pose.h>
#include <AnimationSystem/Skeleton.h>

#include <glm/glm.hpp>

#include <span>
#include <vector>

namespace nous::engine::animation_system
{
    // Composes a LOCAL pose into MODEL-space bone globals.
    //
    // One forward loop, no recursion: global[i] = global[parents[i]] * local[i],
    // and topological order guarantees the parent is already final when child i is
    // reached. This is the entire payoff of the ordering invariant, and it is why
    // an importer that emits bones in aiScene traversal order without sorting
    // produces a rig that looks almost right.
    //
    // Returns false if the pose and skeleton disagree on bone count, or if the
    // skeleton is not topologically sorted -- the second check is O(n) integer
    // comparisons against a bug whose symptom is a silently mangled rig.
    [[nodiscard]] bool BuildGlobals(const SkeletonData& skeleton, const Pose& localPose,
                                    std::vector<glm::mat4>& outGlobals);

    // palette[b] = rootGlobalInverse * globals[b] * offsets[b]
    //
    // `offsets` takes a vertex from mesh space into bone space at bind; `globals[b]`
    // puts it back out in the bone's animated place. So a bone sitting exactly at
    // its bind pose contributes offsets[b]^-1 * offsets[b] == identity, which is
    // the property test_palette pins.
    //
    // rootGlobalInverse is hoisted out of the loop by the caller because it is
    // invariant across bones -- inverting a mat4 per bone per character per frame
    // is the kind of waste that does not show up until there are forty characters.
    // It defaults to identity, which is correct whenever the pose is already
    // model-relative; CAnimator passes inverse(rootEntityGlobal) once the bones
    // live in the scene hierarchy.
    [[nodiscard]] bool BuildPalette(const SkeletonData& skeleton,
                                    std::span<const glm::mat4> globals,
                                    std::vector<glm::mat4>& outPalette,
                                    const glm::mat4& rootGlobalInverse = glm::mat4(1.0f));

    // Convenience: globals then palette, with the caller supplying the globals
    // buffer so it can be kept alive across frames instead of reallocated. The
    // globals are worth keeping anyway -- attachments (a weapon socketed to a hand)
    // read them directly.
    [[nodiscard]] bool BuildPalette(const SkeletonData& skeleton, const Pose& localPose,
                                    std::vector<glm::mat4>& scratchGlobals,
                                    std::vector<glm::mat4>& outPalette,
                                    const glm::mat4& rootGlobalInverse = glm::mat4(1.0f));

    // CPU reference skinning: linear blend skinning over up to four influences.
    //
    // The shipping path is GPU -- upload the palette to a storage buffer and
    // transform in the vertex shader, which moves a few hundred matrices instead of
    // the whole vertex buffer. This stays because it is the reference when the
    // shader output looks wrong, it is the only version that is unit testable, and
    // recomputing a skinned AABB needs it anyway (skinned bounds must be rebuilt
    // per frame or precomputed per clip, or frustum culling pops).
    //
    // Deliberately takes loose spans rather than Vertex3D: naming Vertex3D would
    // mean including Utils/Math/Vertex.inl and giving this library a dependency for
    // one debug helper. The caller de-interleaves; it is a reference path.
    //
    // Weights are used as given and are NOT renormalized -- assimp's
    // aiProcess_LimitBoneWeights already renormalizes at import, and silently
    // fixing them up here would hide an importer that stopped doing so.
    // Normals use the palette's upper 3x3 and are renormalized, which is correct
    // for rigid and uniformly-scaled bones and wrong for non-uniform bone scale
    // (that needs the inverse transpose; no rig here uses it yet).
    //
    // Returns false unless every input span is the same length and every output
    // span is at least that long.
    [[nodiscard]] bool SkinVertices(std::span<const glm::mat4>  palette,
                                    std::span<const glm::vec3>  inPositions,
                                    std::span<const glm::vec3>  inNormals,
                                    std::span<const glm::uvec4> boneIDs,
                                    std::span<const glm::vec4>  boneWeights,
                                    std::span<glm::vec3>        outPositions,
                                    std::span<glm::vec3>        outNormals);
}
