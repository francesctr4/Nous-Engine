#include "ResourceMaterial.h"

#include "ImporterMaterial.h"
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

void ResourceMaterial::IncreaseReferenceCount()
{
	Resource::IncreaseReferenceCount();

	//if (this->GetReferenceCount() > 1)
	//{
	//	diffuseMap.texture->IncreaseReferenceCount();
	//}
}

void ResourceMaterial::DecreaseReferenceCount()
{
	Resource::DecreaseReferenceCount();

	//if (this->GetReferenceCount() >= 1)
	//{
	//	diffuseMap.texture->DecreaseReferenceCount();
	//}
}
