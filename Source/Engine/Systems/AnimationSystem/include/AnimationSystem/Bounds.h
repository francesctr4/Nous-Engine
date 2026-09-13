#pragma once

#include <glm/glm.hpp>

#include <span>

namespace nous::engine::animation_system
{
    // Bounds of a skinned mesh: each bone's BIND-POSE box transformed by that bone's
    // palette matrix, unioned.
    //
    // The box provably contains the deformed mesh. Linear blend skinning is a weighted
    // average whose weights sum to 1, so a skinned vertex lies in the convex hull of
    // its per-bone transformed positions -- and a vertex only has weight on bones whose
    // bind box already contains it, so every one of those points lies inside the union.
    // That is a guarantee, not an approximation that happens to hold, which is what
    // makes it safe for frustum culling: an over-estimate merely draws something
    // skippable, while an under-estimate pops.
    //
    // PER BONE, not one box for the whole mesh. Transforming the whole-mesh box by
    // every matrix satisfies the same proof and was tried first -- but a hand bone then
    // carries a body-sized box out to the hand, and the union comes out several times
    // the character. Correct and useless. Per-bone boxes cost the same ~8 point
    // transforms per bone and stay close to the real silhouette.
    //
    // It still over-estimates, deliberately: a bone's box is its bind extent, so a
    // rotating bone sweeps a box big enough for any orientation. That is the price of
    // not skinning vertices per frame, which is the cost GPU skinning exists to avoid.
    //
    // boneMin/boneMax are indexed by bone ID and may be SHORTER than the palette (a
    // mesh need not use every bone in its skeleton); the extra bones are skipped. A
    // bone whose box is inverted (min > max) has no vertices and is skipped too.
    //
    // The result is in the palette's output space (model space). The caller applies
    // the object's world matrix.
    //
    // Returns false when the palette is empty or no bone contributed, leaving the
    // outputs untouched -- an empty palette is CAnimator's "no usable pose" signal, and
    // a caller must fall back to the bind-pose box rather than treat a zero box as
    // authoritative.
    [[nodiscard]] bool ComputeSkinnedBounds(std::span<const glm::vec3> boneMin,
                                            std::span<const glm::vec3> boneMax,
                                            std::span<const glm::mat4> palette,
                                            glm::vec3& outMin,
                                            glm::vec3& outMax);
}
