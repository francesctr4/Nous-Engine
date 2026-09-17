#pragma once

#include <glm/glm.hpp>

#include <span>

namespace nous::engine::animation_system
{
    // Bounds of a skinned mesh: each bone's BIND-POSE box transformed by that bone's
    // palette matrix, unioned.
    //
    // The result provably contains the deformed mesh: LBS weights sum to 1, so a skinned
    // vertex lies in the convex hull of its per-bone transformed positions, and a vertex
    // only has weight on bones whose bind box already contains it. A guarantee rather than
    // an approximation, which is what makes it safe for culling -- over-estimating draws
    // something skippable, under-estimating pops.
    //
    // PER BONE, not one box for the whole mesh. The whole-mesh version satisfies the same
    // proof and was tried first, but a hand bone then carries a body-sized box out to the
    // hand and the union comes out several times the character. It still over-estimates
    // deliberately, since a bone's box is its bind extent and a rotating bone sweeps enough
    // for any orientation -- the price of not skinning vertices per frame.
    //
    // boneMin/boneMax are indexed by bone ID and may be SHORTER than the palette; the
    // extras are skipped, as is an inverted box (min > max), which means no vertices.
    //
    // Returns false when the palette is empty or no bone contributed, leaving the outputs
    // untouched -- the caller must fall back to the bind-pose box rather than treat a zero
    // box as authoritative. The result is in the palette's output space (model space).
    [[nodiscard]] bool ComputeSkinnedBounds(std::span<const glm::vec3> boneMin,
                                            std::span<const glm::vec3> boneMax,
                                            std::span<const glm::mat4> palette,
                                            glm::vec3& outMin,
                                            glm::vec3& outMax);
}
