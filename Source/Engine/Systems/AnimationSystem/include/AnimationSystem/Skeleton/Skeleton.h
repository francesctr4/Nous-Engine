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
    // The skeleton as plain data. ResourceSkeleton (step 6, under
    // ResourceManager/Types/) will hold one of these BY VALUE rather than
    // redeclaring the same fields, and SkeletonExtract::BuildSkeleton returns one.
    // The spec listed SkeletonData and ResourceSkeleton separately with identical
    // members; one type composed into the resource is the same design with one
    // fewer place to drift.
    //
    // ORDER IS TOPOLOGICAL: parents[i] < i for every bone, roots are -1. That is
    // the invariant making BuildGlobals a single forward loop -- no recursion, no
    // visited set, no dirty-flag chasing. Importers must guarantee it;
    // IsTopologicallySorted() exists so an import-time assert or a test can prove
    // it rather than assume it.
    struct SkeletonData
    {
        std::vector<std::string> names;
        std::vector<int>         parents;      // -1 for a root bone
        std::vector<glm::mat4>   offsets;      // mesh space -> bone space, bind pose

        // Bind-pose LOCAL transform per bone -- what a bone holds when no channel
        // drives it. Not in the spec's field list, and it has to be here: Sample()
        // must fill EVERY bone of the pose, including joints a given clip never
        // touches, and `offsets` alone only gives the inverse GLOBAL bind. Deriving
        // locals from offsets costs an inverse per bone plus a parent-chain walk;
        // at import time it is just the node's local matrix, already to hand.
        //
        // It also makes the suite's most valuable test trivially expressible: feed
        // bindLocals in as the pose, get identity matrices out of BuildPalette.
        std::vector<Transform>   bindLocals;

        // name -> index. Rebuilt on load, never serialized: a string map in a binary
        // is dead weight next to the names array it duplicates.
        std::unordered_map<std::string, uint32_t> lookup;

        [[nodiscard]] size_t BoneCount() const { return names.size(); }

        // -1 when the name is unknown. Signed on purpose -- callers branch on the
        // miss, and an unsigned sentinel invites `if (index)` bugs at index 0.
        [[nodiscard]] int FindBone(std::string_view boneName) const;

        // Fills `lookup` from `names`. Call after deserializing, or after building
        // the arrays by hand.
        void RebuildLookup();

        // Cheap structural checks, for import-time asserts and for tests.
        [[nodiscard]] bool IsConsistent() const;            // all arrays same length
        [[nodiscard]] bool IsTopologicallySorted() const;   // parents[i] < i, roots -1
    };
}
