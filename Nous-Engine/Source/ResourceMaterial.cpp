#include "ResourceMaterial.h"

#include "ImporterMaterial.h"
#include "ResourceTexture.h"

ResourceMaterial::ResourceMaterial(UID uid) : Resource(uid, ResourceType::MATERIAL)
{

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
