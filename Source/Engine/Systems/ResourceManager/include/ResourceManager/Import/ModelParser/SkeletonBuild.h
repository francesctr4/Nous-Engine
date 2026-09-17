#pragma once

#include <AnimationSystem/Skeleton/Skeleton.h>
#include <AnimationSystem/Transform.h>

#include <glm/glm.hpp>

#include <expected>
#include <string>
#include <unordered_set>
#include <vector>

// Pure half of the model pre-pass: a raw node hierarchy in, a SkeletonData out.
//
// NO ASSIMP TYPES APPEAR HERE, and that is the point. ModelParser.cpp is the only
// translation unit in the engine that includes <assimp/*>; it walks the aiScene into the
// flat arrays below and hands them over, so everything with an off-by-one in it lives on
// this side of the line where a test can reach it with hand-built input.
namespace nous::engine::resource_manager
{
    // One node of the source hierarchy, in DEPTH-FIRST order: `parent` must be a smaller
    // index than the node's own. A DFS from the scene root satisfies that by construction,
    // and BuildSkeleton validates it rather than assuming it.
    struct RawBoneNode
    {
        std::string name;
        int         parent = -1;      // index into the same array, -1 for the root

        // Where SkeletonData::bindLocals comes from, and the reason the pre-pass walks
        // NODES rather than collecting aiBones: an aiBone gives an offset matrix and
        // nothing else, so a bone-only pass would have to invert its way back to the local
        // bind through the parent chain.
        animation_system::Transform localBind;

        // True when some aiBone names this node. False for a joint that carries no weights
        // -- kept anyway when it has a bone descendant, since dropping it would put a hole
        // in the parent chain.
        bool      isBone = false;
        glm::mat4 offset{ 1.0f };     // aiBone::mOffsetMatrix; ignored when !isBone

        // True when an animation channel drives this node. Set ONLY by
        // ApplyAnimatedFallback, never by the aiScene walk -- see the gate documented
        // there. It is the only signal a bone-free model offers about which nodes are
        // joints: an anim-only export contains no aiMesh, therefore no aiBone and no
        // offset matrices at all, yet its node hierarchy is a complete rig.
        bool isAnimated = false;
    };

    // Prunes, renumbers and validates.
    //
    // KEEPS a node when it is a bone or an ancestor of one, so a scene's cameras, lights
    // and geometry nodes do not become skeleton entries. Survivors keep their relative
    // order, so the result is still topological and SkeletonData's parents[i] < i holds by
    // construction rather than by a sort afterwards.
    //
    // OFFSETS: a real bone keeps assimp's own offset matrix, which is authoritative and can
    // legitimately differ from what the node chain implies. A kept NON-bone has none of its
    // own, so it gets inverse(global bind) accumulated down the chain -- which means
    // nothing in the palette is silently identity.
    //
    // Returns an error string rather than a bool because "which node" is the whole
    // diagnostic at import time.
    [[nodiscard]] std::expected<animation_system::SkeletonData, std::string>
    BuildSkeleton(const std::vector<RawBoneNode>& nodes);

    // Marks channel-named nodes as skeleton members, PLUS all their descendants -- but ONLY
    // when the hierarchy contains no real bone at all. THAT GATE IS LOAD-BEARING.
    //
    // Descendants are included because BuildSkeleton already keeps ancestors, so the two
    // together make the skeleton the whole subtree SPANNED by the animated nodes. A clip
    // does not drive every joint in its rig -- Mixamo animates 52 of 65, the rest being
    // "_End" terminators -- so keeping only the driven ones loses joints the file contains.
    //
    // Applied unconditionally it would promote any animated non-bone (root motion on a
    // geometry node, a prop parented to a hand, an exporter's helper) into an extra bone
    // with a derived offset: silently changing rigs that import correctly today, and
    // breaking the property that a skinned FBX and its anim-only sibling yield identical
    // bone-name lists -- which is what name-based clip binding rests on.
    //
    // So: bones win. This is a fallback for when there is nothing else, not an additional
    // source of joints.
    void ApplyAnimatedFallback(std::vector<RawBoneNode>& nodes,
                               const std::unordered_set<std::string>& animatedNames);
}
