#pragma once

#include <AnimationSystem/Skeleton.h>
#include <AnimationSystem/Transform.h>

#include <glm/glm.hpp>

#include <expected>
#include <string>
#include <vector>

// Pure half of the model pre-pass: turning a raw node hierarchy into a
// SkeletonData. NO ASSIMP TYPES APPEAR HERE -- that is the point. ModelParser.cpp
// is the only translation unit in the engine that includes <assimp/*>, and it is
// deliberately thin: it walks the aiScene into the flat arrays below and hands
// them here. Everything with an off-by-one in it lives on this side of that line,
// where t_ResourceManager_SkeletonBuild can reach it with hand-built input.
namespace nous::engine::resource_manager
{
    // One node of the source hierarchy, in DEPTH-FIRST order: `parent` must be a
    // smaller index than the node's own. A DFS from the scene root satisfies that
    // by construction, which is why the traversal side has no sorting to do and no
    // sorting to get wrong -- BuildSkeleton validates it rather than assuming it.
    struct RawBoneNode
    {
        std::string name;
        int         parent = -1;      // index into the same array, -1 for the root

        // The node's own local transform. This is where SkeletonData::bindLocals
        // comes from, and it is the reason the pre-pass walks NODES rather than
        // collecting aiBones: an aiBone gives you an offset matrix and nothing
        // else, so a bone-only pass would have to invert its way back to the local
        // bind through the parent chain.
        animation_system::Transform localBind;

        // True when some aiBone in some mesh names this node. False for a joint
        // that exists in the hierarchy but carries no weights -- kept anyway when
        // it has a bone descendant, because dropping it would put a hole in the
        // parent chain.
        bool      isBone = false;
        glm::mat4 offset{ 1.0f };     // aiBone::mOffsetMatrix; ignored when !isBone
    };

    // Prunes, renumbers and validates.
    //
    // KEEPS a node when it is a bone or an ancestor of one; drops everything else,
    // so a scene's cameras, lights and geometry nodes do not become skeleton
    // entries. Survivors keep their relative order, so the result is still
    // topological (a subsequence of a topological order is topological) and
    // SkeletonData's parents[i] < i invariant holds by construction rather than by
    // a sort afterwards.
    //
    // OFFSETS: a real bone keeps assimp's own offset matrix, which is
    // authoritative and can legitimately differ from what the node chain implies
    // (the mesh may carry its own transform). A kept NON-bone has no offset of its
    // own, so it gets inverse(global bind) accumulated down the chain -- the
    // correct generalization, and it means nothing in the palette is silently
    // identity.
    //
    // Returns an error string rather than a bool because the failure is worth
    // naming at import time: "which node" is the whole diagnostic.
    [[nodiscard]] std::expected<animation_system::SkeletonData, std::string>
    BuildSkeleton(const std::vector<RawBoneNode>& nodes);
}
