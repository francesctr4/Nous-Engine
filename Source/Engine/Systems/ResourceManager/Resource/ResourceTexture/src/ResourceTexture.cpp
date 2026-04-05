#include "Engine/Systems/ResourceManager/Resource/ResourceTexture/include/ResourceTexture.h"

ResourceTexture::ResourceTexture(UID uid) : Resource(uid, ResourceType::TEXTURE)
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
