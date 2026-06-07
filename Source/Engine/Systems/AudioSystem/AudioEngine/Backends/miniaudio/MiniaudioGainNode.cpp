#include "Engine/Systems/AudioSystem/AudioEngine/Backends/miniaudio/MiniaudioGainNode.h"
#include "Engine/Systems/AudioSystem/AudioGraph/GainDsp.h"

namespace
{
    void GainNodeProcess(ma_node* pNode, const float** ppFramesIn, ma_uint32* pFrameCountIn,
                         float** ppFramesOut, ma_uint32* pFrameCountOut)
    {
        auto* g = reinterpret_cast<MiniaudioGainNode*>(pNode);
        // 1:1 rate (no resampling): process exactly the output frame count. miniaudio
        // pre-mixes inputs into ppFramesIn[0] and posts silence if nothing is attached.
        GainApply(ppFramesOut[0], ppFramesIn[0], *pFrameCountOut, g->channels, g->gain);
        (void)pFrameCountIn;
    }

    ma_node_vtable g_gainVTable = {
        GainNodeProcess,
        nullptr,   // no custom required-input-frame-count calc (1:1)
        1,         // input buses
        1,         // output buses
        0          // flags
    };
}

ma_result MiniaudioGainNodeInit(ma_node_graph* graph, ma_uint32 channels, float gain, MiniaudioGainNode* node)
{
    node->channels = channels;
    node->gain     = gain;

    ma_node_config cfg = ma_node_config_init();
    cfg.vtable          = &g_gainVTable;
    cfg.pInputChannels  = &node->channels;   // 1 input bus  → 1 channel-count entry
    cfg.pOutputChannels = &node->channels;   // 1 output bus → 1 channel-count entry
    return ma_node_init(graph, &cfg, nullptr, &node->baseNode);
}

void MiniaudioGainNodeUninit(MiniaudioGainNode* node)
{
    ma_node_uninit(&node->baseNode, nullptr);
}
