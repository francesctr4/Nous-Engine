#pragma once

#include <AnimationSystem/Transform.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace nous::engine::animation_system
{
    // The skeleton as plain data. ResourceSkeleton holds one of these BY VALUE rather than
    // redeclaring the same fields, which keeps the two from drifting.
    //
    // ORDER IS TOPOLOGICAL: parents[i] < i for every bone, roots are -1. That invariant is
    // what makes BuildGlobals a single forward loop -- no recursion, no visited set, no
    // dirty-flag chasing. Importers must guarantee it; IsTopologicallySorted() exists so an
    // assert or a test can prove it rather than assume it.
    struct SkeletonData
    {
        std::vector<std::string> names;
        std::vector<int>         parents;      // -1 for a root bone
        std::vector<glm::mat4>   offsets;      // mesh space -> bone space, bind pose

        // Bind-pose LOCAL transform per bone -- what a bone holds when no channel drives it.
        // Sample() must fill EVERY bone, including joints a clip never touches, and
        // `offsets` alone only gives the inverse GLOBAL bind: deriving locals from it costs
        // an inverse per bone plus a parent-chain walk, while at import time it is just the
        // node's local matrix. Without it, undriven joints collapse to the origin, and that
        // reads as a skinning bug rather than a sampling one.
        std::vector<Transform>   bindLocals;

        // name -> index. Rebuilt on load, never serialized: a string map in a binary is dead
        // weight next to the names array it duplicates.
        std::unordered_map<std::string, uint32_t> lookup;

        [[nodiscard]] size_t BoneCount() const { return names.size(); }

        // -1 when unknown. Signed on purpose -- callers branch on the miss, and an unsigned
        // sentinel invites `if (index)` bugs at index 0.
        [[nodiscard]] int FindBone(std::string_view boneName) const;

        // Fills `lookup` from `names`. Call after deserializing or after building the arrays
        // by hand.
        void RebuildLookup();

        [[nodiscard]] bool IsConsistent() const;            // all arrays same length
        [[nodiscard]] bool IsTopologicallySorted() const;   // parents[i] < i, roots -1
    };
}
