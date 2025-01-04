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
	//return ImporterMaterial::Load(this->GetLibraryPath().c_str(), this);
	return true;
}

bool ResourceMaterial::UnloadFromMemory()
{
	return false;
}