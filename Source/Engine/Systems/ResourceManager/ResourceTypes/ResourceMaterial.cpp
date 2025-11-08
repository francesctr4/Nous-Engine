#include <Engine/Systems/ResourceManager/ResourceTypes/ResourceMaterial.h>
#include <Engine/Systems/ResourceManager/ResourceTypes/ResourceTexture.h>

ResourceMaterial::ResourceMaterial(UID uid) : Resource(uid, ResourceType::MATERIAL)
{
	ID = INVALID_ID;
	internalID = INVALID_ID;
	generation = INVALID_ID;
}

ResourceMaterial::~ResourceMaterial()
{

}
