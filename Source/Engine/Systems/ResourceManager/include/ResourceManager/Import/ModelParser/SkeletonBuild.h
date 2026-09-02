#pragma once

#include <AnimationSystem/Skeleton.h>
#include <AnimationSystem/Transform.h>

#include <glm/glm.hpp>

#include <expected>
#include <string>
#include <unordered_set>
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

        // True when an animation channel drives this node. Set ONLY by
        // ApplyAnimatedFallback below, never by the aiScene walk directly -- see the
        // gate documented there.
        //
        // This is the only signal a bone-free model offers about which nodes are
        // joints. An anim-only export (Mixamo "without skin") contains no aiMesh, so
        // it contains no aiBone and no offset matrices at all, yet its node
        // hierarchy is a complete rig.
        bool isAnimated = false;
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

    // Marks nodes named by an animation channel as skeleton members, PLUS all of
    // their descendants -- but ONLY when the hierarchy contains no real bone at all.
    // THAT GATE IS LOAD-BEARING.
    //
    // Descendants are included because BuildSkeleton already keeps ancestors, so the
    // two together make the skeleton the whole subtree SPANNED by the animated
    // nodes. A clip does not drive every joint in its rig -- Mixamo's anim-only
    // export animates 52 of 65, the rest being "_End" leaf terminators -- and
    // keeping only the driven ones produces a skeleton missing joints the file
    // plainly contains.
    //
    // Applied unconditionally, this promotes any animated non-bone node -- root
    // motion on a geometry node, a prop parented to a hand, an exporter's helper --
    // into an extra "bone" carrying a derived, non-authoritative offset. It would
    // silently change skeletons that import correctly today, and it would break the
    // property that a skinned FBX and its anim-only sibling yield identical bone-name
    // lists, which is what name-based clip binding rests on.
    //
    // So: bones win. This is a fallback for the case where there is nothing else,
    // not an additional source of joints. Pinned by
    // t_SkeletonBuild.AnimatedFallbackIsIgnoredEntirelyWhenAnyBoneExists.
    void ApplyAnimatedFallback(std::vector<RawBoneNode>& nodes,
                               const std::unordered_set<std::string>& animatedNames);
}
