#pragma once

#include <AnimationSystem/Pose.h>
#include <AnimationSystem/Skeleton/Skeleton.h>

#include <glm/glm.hpp>

#include <span>
#include <vector>

namespace nous::engine::animation_system
{
    // Composes a LOCAL pose into MODEL-space bone globals.
    //
    // One forward loop, no recursion: global[i] = global[parents[i]] * local[i], because
    // topological order guarantees the parent is final when child i is reached. That is
    // the entire payoff of the ordering invariant.
    //
    // Returns false if the pose and skeleton disagree on bone count, or the skeleton is
    // not topologically sorted -- O(n) integer compares against a bug whose only symptom
    // is a silently mangled rig.
    [[nodiscard]] bool BuildGlobals(const SkeletonData& skeleton, const Pose& localPose,
                                    std::vector<glm::mat4>& outGlobals);

    // palette[b] = rootGlobalInverse * globals[b] * offsets[b]
    //
    // `offsets` takes a vertex from mesh space into bone space at bind; globals[b] puts it
    // back in the bone's animated place. So a bone at its bind pose contributes identity.
    //
    // rootGlobalInverse is hoisted out of the loop by the caller because it is invariant
    // across bones. It defaults to identity, which is correct whenever the pose is already
    // model-relative.
    [[nodiscard]] bool BuildPalette(const SkeletonData& skeleton,
                                    std::span<const glm::mat4> globals,
                                    std::vector<glm::mat4>& outPalette,
                                    const glm::mat4& rootGlobalInverse = glm::mat4(1.0f));

    // Convenience: globals then palette, with the caller supplying the globals buffer so
    // it can be kept across frames. They are worth keeping anyway -- bone attachments read
    // them directly.
    [[nodiscard]] bool BuildPalette(const SkeletonData& skeleton, const Pose& localPose,
                                    std::vector<glm::mat4>& scratchGlobals,
                                    std::vector<glm::mat4>& outPalette,
                                    const glm::mat4& rootGlobalInverse = glm::mat4(1.0f));

    // CPU reference skinning: linear blend over up to four influences.
    //
    // The shipping path is GPU. This stays because it is the reference when the shader
    // output looks wrong, it is the only unit-testable version, and skinned-bounds work
    // needs it. It takes loose spans rather than Vertex3D on purpose -- naming Vertex3D
    // would give this library a dependency for one debug helper.
    //
    // Weights are used as given and NOT renormalized: assimp's aiProcess_LimitBoneWeights
    // already does that at import, and fixing them up here would hide an importer that
    // stopped. Normals use the palette's upper 3x3 and are renormalized, which is correct
    // for rigid and uniformly-scaled bones and wrong for non-uniform bone scale.
    //
    // Returns false unless every input span is the same length and every output span is at
    // least that long.
    [[nodiscard]] bool SkinVertices(std::span<const glm::mat4>  palette,
                                    std::span<const glm::vec3>  inPositions,
                                    std::span<const glm::vec3>  inNormals,
                                    std::span<const glm::uvec4> boneIDs,
                                    std::span<const glm::vec4>  boneWeights,
                                    std::span<glm::vec3>        outPositions,
                                    std::span<glm::vec3>        outNormals);
}
