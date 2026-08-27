#include <ResourceManager/Types/ResourceTexture/ResourceTexture.h>

ResourceTexture::ResourceTexture(uint32 uid) : ResourceBase(uid, ResourceType::TEXTURE)
{
    internalData = nullptr;
	generation = INVALID_ID;

    width = 0;
    height = 0;
    channelCount = 0;

    hasTransparency = false;
}

ResourceTexture::~ResourceTexture()
{
}
