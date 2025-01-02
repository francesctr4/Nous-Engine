#include "ResourceTexture.h"

ResourceTexture::ResourceTexture(UID uid) : Resource(uid, ResourceType::TEXTURE)
{
}

ResourceTexture::~ResourceTexture()
{
}

bool ResourceTexture::LoadInMemory()
{
	return false;
}

bool ResourceTexture::UnloadFromMemory()
{
	return false;
}
