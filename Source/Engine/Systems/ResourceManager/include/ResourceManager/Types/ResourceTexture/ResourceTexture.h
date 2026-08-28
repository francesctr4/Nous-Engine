#ifndef RESOURCETEXTURE_H
#define RESOURCETEXTURE_H

#include <ResourceManager/Core/ResourceBase.h>
#include <vector>
#include <cstdint>

class ResourceTexture : public ResourceBase
{
public:

	// Constructor & Destructor

	NOUS_ENGINE_API ResourceTexture(uint32_t uid = 0);
	NOUS_ENGINE_API ~ResourceTexture() override;

public:

    // Incremented each time the texture is (re)uploaded to the GPU.
    // Used by WriteInstanceSampler to skip redundant vkUpdateDescriptorSets calls:
    // if generation and resource ID haven't changed since the last write, the update is skipped.
    uint32_t generation;

    // Owning pointer to the backend's GPU image (e.g. VulkanImage*).
    // Allocated and freed exclusively by the renderer backend.
    void* internalData;

    uint32_t width;
    uint32_t height;

    uint8_t channelCount;
    bool hasTransparency;

    // Temporary CPU pixel buffer — populated by ImporterTexture::Deserialize,
    // consumed and cleared by ImporterTexture::Upload/Evict.
    std::vector<uint8_t> pixelData;
};

#endif // RESOURCETEXTURE_H