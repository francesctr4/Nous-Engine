#pragma once

#include "Engine/Systems/AudioSystem/AudioGraph/AudioEffectTypes.h"

#include <miniaudio.h>
#include <vector>

// Owns the ma_nodes for one per-voice effect chain and the wiring that splices it
// between a source node (a ma_sound) and an output node (a bus group / endpoint).
// Lives under Backends/miniaudio/ because it names ma_* directly. The handle the
// backend hands out (EffectChainHandle) is a MiniaudioEffectChain*.
class MiniaudioEffectChain
{
public:
    // Builds nodes from `desc` and wires: source → node[0] → ... → node[N-1] → output.
    // On any node-init failure, frees what was built and returns false WITHOUT touching
    // `source` (it stays attached to its original bus). Returns false for an empty desc.
    bool Build(ma_engine* engine, ma_node* source, const AudioGraphDesc& desc, ma_node* output);

    // Reattaches `source` straight to `output`, then uninits + frees the nodes.
    // `source` and `output` must still be alive.
    void Destroy() noexcept;

    // Live param update on one node. paramIndex matches the effect's schema order.
    void SetParam(int effectIndex, int paramIndex, float value);

private:
    struct ChainNode { AudioEffectType type; ma_node* node; };

    ma_node* CreateNode(ma_node_graph* graph, const AudioEffectDesc& desc);
    void     DestroyNode(const ChainNode& cn) noexcept;
    void     CleanupNodes() noexcept;

    ma_engine*             m_engine     = nullptr;
    ma_node*               m_source     = nullptr;
    ma_node*               m_output     = nullptr;
    ma_uint32              m_channels   = 0;
    ma_uint32              m_sampleRate = 0;
    std::vector<ChainNode> m_nodes;
};
