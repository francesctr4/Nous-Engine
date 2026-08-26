#pragma once

#include <AudioSystem/AudioGraph/AudioEffectTypes.h>

#include <cstdint>
#include <unordered_map>
#include <vector>

// Pure, runtime-free linearization of an editor graph into an ordered effect chain.
// Operates on plain node/link descriptors (no imgui-node-editor types) so it is
// unit-testable headless AND reused verbatim by the editor (tested code == run code).
// Assumes a linear chain: each node has at most one outgoing link.
namespace nous::audio_editor
{
    enum class LinNodeKind { Source, Output, Effect };

    struct LinNode
    {
        uint64_t           id;
        LinNodeKind        kind;
        AudioEffectType    effectType;   // valid when kind == Effect
        std::vector<float> params;
    };

    struct LinLink
    {
        uint64_t fromNodeId;   // node owning the output side of the link
        uint64_t toNodeId;     // node owning the input side of the link
    };

    // Fills outOrderedIds with the node ids along Source → ... → Output, inclusive of
    // both anchors. Returns false if there is no Source/Output, or the path from
    // Source does not reach Output (disconnected / broken chain / cycle).
    inline bool LinearizeOrder(const std::vector<LinNode>& nodes,
                               const std::vector<LinLink>& links,
                               std::vector<uint64_t>& outOrderedIds)
    {
        outOrderedIds.clear();

        const LinNode* source = nullptr;
        const LinNode* output = nullptr;
        std::unordered_map<uint64_t, const LinNode*> byId;
        for (const LinNode& n : nodes)
        {
            byId[n.id] = &n;
            if (n.kind == LinNodeKind::Source) source = &n;
            if (n.kind == LinNodeKind::Output) output = &n;
        }
        if (!source || !output)
            return false;

        std::unordered_map<uint64_t, uint64_t> next;   // fromNodeId → toNodeId
        for (const LinLink& l : links)
            next[l.fromNodeId] = l.toNodeId;

        uint64_t cur = source->id;
        outOrderedIds.push_back(cur);

        const size_t guard = nodes.size() + 1;   // cycle / runaway guard
        for (size_t steps = 0; steps <= guard; ++steps)
        {
            auto it = next.find(cur);
            if (it == next.end())
                return false;                     // dead-ends before Output

            const uint64_t nid = it->second;
            auto bit = byId.find(nid);
            if (bit == byId.end())
                return false;                     // link to a missing node

            outOrderedIds.push_back(nid);
            if (bit->second->kind == LinNodeKind::Output)
                return true;                      // reached Output — done

            cur = nid;
        }
        return false;                             // never reached Output
    }

    // Convenience: ordered effect nodes → AudioGraphDesc (params copied in chain order).
    inline bool Linearize(const std::vector<LinNode>& nodes,
                          const std::vector<LinLink>& links,
                          AudioGraphDesc& outDesc)
    {
        outDesc.clear();

        std::vector<uint64_t> order;
        if (!LinearizeOrder(nodes, links, order))
            return false;

        std::unordered_map<uint64_t, const LinNode*> byId;
        for (const LinNode& n : nodes)
            byId[n.id] = &n;

        for (uint64_t id : order)
        {
            const LinNode* n = byId.at(id);
            if (n->kind == LinNodeKind::Effect)
                outDesc.push_back({ n->effectType, n->params });
        }
        return true;
    }
}
