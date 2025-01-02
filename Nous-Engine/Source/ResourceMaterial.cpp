#include "ResourceMaterial.h"

#include "ImporterMaterial.h"

ResourceMaterial::ResourceMaterial(UID uid) : Resource(uid, ResourceType::MATERIAL)
{

}

ResourceMaterial::~ResourceMaterial()
{

}

bool ResourceMaterial::LoadInMemory()
{
	//ImporterMaterial::Load();

	IncreaseReferenceCount();

	return false;
}

bool ResourceMaterial::UnloadFromMemory()
{
	return false;
}