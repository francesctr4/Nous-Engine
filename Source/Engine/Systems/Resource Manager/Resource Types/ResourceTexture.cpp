#include "ResourceTexture.h"

ResourceTexture::ResourceTexture(UID uid) : Resource(uid, ResourceType::TEXTURE)
{
	ID = INVALID_ID;
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
