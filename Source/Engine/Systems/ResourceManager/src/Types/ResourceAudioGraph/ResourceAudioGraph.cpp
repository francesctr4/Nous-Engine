#include <ResourceManager/Types/ResourceAudioGraph/ResourceAudioGraph.h>

ResourceAudioGraph::ResourceAudioGraph(const uint32_t uid)
    : ResourceBase(uid, ResourceType::AUDIO_GRAPH)
{
}

ResourceAudioGraph::~ResourceAudioGraph() = default;
