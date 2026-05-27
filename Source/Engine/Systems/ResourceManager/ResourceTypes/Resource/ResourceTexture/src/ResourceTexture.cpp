#include "Engine/Systems/ResourceManager/ResourceTypes/Resource/ResourceTexture/include/ResourceTexture.h"

ResourceTexture::ResourceTexture(uint32 uid) : Resource(uid, ResourceType::TEXTURE)
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
