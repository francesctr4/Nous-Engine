#include <ResourceManager/Import/ModelParser/SkeletonBuild.h>

#include <format>

namespace nous::engine::resource_manager
{
    using animation_system::SkeletonData;

    std::expected<SkeletonData, std::string> BuildSkeleton(const std::vector<RawBoneNode>& nodes)
    {
        const size_t count = nodes.size();

        // Validate the DFS-order contract up front. Everything below indexes
        // parents freely on the strength of it, so checking here is what makes the
        // rest of this function able to be a pair of flat loops.
        for (size_t i = 0; i < count; ++i)
        {
            const int parent = nodes[i].parent;

            if (parent < 0) continue;

            if (static_cast<size_t>(parent) >= i)
            {
                return std::unexpected(std::format(
                    "node {} '{}' names parent {} which does not come before it — "
                    "the node array must be in depth-first order",
                    i, nodes[i].name, parent));
            }
        }

        // Mark bones, then propagate the mark up every parent chain. Reverse order
        // is what makes one pass enough: in DFS order a child always sits after its
        // parent, so walking backwards means a node is visited only after every
        // descendant that could have marked it already has.
        std::vector<bool> keep(count, false);
        for (size_t i = 0; i < count; ++i) keep[i] = nodes[i].isBone;

        for (size_t i = count; i-- > 0; )
        {
            if (keep[i] && nodes[i].parent >= 0) keep[static_cast<size_t>(nodes[i].parent)] = true;
        }

        // Forward pass assigns new indices in old order, so the survivors are a
        // subsequence of a topological order — which is still topological. No sort,
        // and nothing to get wrong in one.
        std::vector<int> oldToNew(count, -1);
        int next = 0;
        for (size_t i = 0; i < count; ++i)
        {
            if (keep[i]) oldToNew[i] = next++;
        }

        SkeletonData skeleton;
        const auto kept = static_cast<size_t>(next);

        skeleton.names.reserve(kept);
        skeleton.parents.reserve(kept);
        skeleton.bindLocals.reserve(kept);
        skeleton.offsets.resize(kept, glm::mat4(1.0f));

        // Global bind per KEPT bone, accumulated as we go. Only needed to derive an
        // offset for a kept non-bone, but computing it inline costs one multiply
        // per bone and saves a second pass.
        std::vector<glm::mat4> globalBind;
        globalBind.reserve(kept);

        for (size_t i = 0; i < count; ++i)
        {
            if (!keep[i]) continue;

            const RawBoneNode& node = nodes[i];
            const int newParent = node.parent < 0
                ? -1
                : oldToNew[static_cast<size_t>(node.parent)];

            // A kept node's parent is always kept: the reverse pass above marked
            // every ancestor of every marked node. If this ever fires, that pass or
            // the DFS-order contract is broken, not this line.
            if (node.parent >= 0 && newParent < 0)
            {
                return std::unexpected(std::format(
                    "internal: kept node '{}' has a dropped parent '{}'",
                    node.name, nodes[static_cast<size_t>(node.parent)].name));
            }

            const size_t newIndex = skeleton.names.size();

            skeleton.names.push_back(node.name);
            skeleton.parents.push_back(newParent);
            skeleton.bindLocals.push_back(node.localBind);

            const glm::mat4 local = node.localBind.ToMatrix();
            globalBind.push_back(newParent < 0
                ? local
                : globalBind[static_cast<size_t>(newParent)] * local);

            // A real bone keeps assimp's offset — authoritative, and it can
            // legitimately differ from what the node chain implies because the mesh
            // may carry its own transform. A kept non-bone has none, so derive it;
            // leaving identity there would silently misplace anything that later
            // referenced the joint.
            skeleton.offsets[newIndex] = node.isBone
                ? node.offset
                : glm::inverse(globalBind[newIndex]);
        }

        skeleton.RebuildLookup();

        return skeleton;
    }
}
