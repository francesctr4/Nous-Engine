#pragma once

#include <miniaudio.h>

// Custom 1-in / 1-out passthrough node that scales its input by `gain`. The only
// MVP effect with no stock ma_*_node equivalent. Also serves as the reference
// custom-node implementation for the future verblib ReverbNode.
struct MiniaudioGainNode
{
    ma_node_base baseNode;   // MUST be the first member — makes this a valid ma_node.
    ma_uint32    channels;
    float        gain;       // written (main thread) / read (audio thread); benign race for a float.
};

ma_result MiniaudioGainNodeInit(ma_node_graph* graph, ma_uint32 channels, float gain, MiniaudioGainNode* node);
void      MiniaudioGainNodeUninit(MiniaudioGainNode* node);
