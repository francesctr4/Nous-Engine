#ifndef RESOURCETEXTURE_H
#define RESOURCETEXTURE_H

#include "Engine/Core/Globals.h"
#include "Engine/Systems/ResourceManager/Resource/Resource.h"
#include <vector>

class ResourceTexture : public Resource
{
public:

	// Constructor & Destructor

	NOUS_ENGINE_API ResourceTexture(uint32 uid = 0);
	NOUS_ENGINE_API ~ResourceTexture() override;

public:

    uint32 generation;
    void* internalData;

    uint32 width;
    uint32 height;

    uint8 channelCount;
    bool hasTransparency;

    // Temporary CPU pixel buffer — populated by ImporterTexture::Deserialize,
    // consumed and cleared by ImporterTexture::Upload/Evict.
    std::vector<uint8_t> pixelData;
};

#endif // RESOURCETEXTURE_H