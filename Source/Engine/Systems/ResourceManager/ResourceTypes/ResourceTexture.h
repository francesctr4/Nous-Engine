#ifndef RESOURCETEXTURE_H
#define RESOURCETEXTURE_H

#include <Engine/Core/Globals.h>
#include <Engine/Systems/ResourceManager/ResourceTypes/Resource.h>
#include <Engine/Renderer/RendererTypes.h>

class ResourceTexture : public Resource
{
public:

	// Constructor & Destructor

	ResourceTexture(UID uid = 0);
	~ResourceTexture() override;

public:

    uint32 ID;
    uint32 generation;
    void* internalData;

    uint32 width;
    uint32 height;

    uint8 channelCount;
    bool hasTransparency;
};

#endif // RESOURCETEXTURE_H