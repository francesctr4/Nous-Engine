#include <ResourceManager/Types/ResourceAudioGraph/ResourceAudioGraph.h>

ResourceAudioGraph::ResourceAudioGraph(const uint32 uid)
    : ResourceBase(uid, ResourceType::AUDIO_GRAPH)
{
}

ResourceAudioGraph::~ResourceAudioGraph() = default;
