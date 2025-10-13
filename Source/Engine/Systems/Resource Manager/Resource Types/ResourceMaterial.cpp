#include "ResourceMaterial.h"
#include "ResourceTexture.h"

ResourceMaterial::ResourceMaterial(UID uid) : Resource(uid, ResourceType::MATERIAL)
{
	ID = INVALID_ID;
	internalID = INVALID_ID;
	generation = INVALID_ID;
}

ResourceMaterial::~ResourceMaterial()
{

}
