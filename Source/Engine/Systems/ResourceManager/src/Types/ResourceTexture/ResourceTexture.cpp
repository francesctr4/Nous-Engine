#include <ResourceManager/Types/ResourceTexture/ResourceTexture.h>
#include <EngineCore/InvalidID.h>

ResourceTexture::ResourceTexture(uint32_t uid) : ResourceBase(uid, ResourceType::TEXTURE)
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
