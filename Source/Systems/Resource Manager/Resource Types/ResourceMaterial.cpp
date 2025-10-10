#include "Systems/Resource Manager/Resource Types/ResourceMaterial.h"
#include "Systems/Resource Manager/Resource Types/ResourceTexture.h"

ResourceMaterial::ResourceMaterial(UID uid) : Resource(uid, ResourceType::MATERIAL)
{
	ID = INVALID_ID;
	internalID = INVALID_ID;
	generation = INVALID_ID;
}

ResourceMaterial::~ResourceMaterial()
{

}
