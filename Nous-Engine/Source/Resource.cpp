#include "Resource.h"

std::map<ResourceType, std::string> resourceTypeToString
{
	{ResourceType::MESH, "nmesh"},
	{ResourceType::MATERIAL, "nmat"},
	{ResourceType::TEXTURE, "png"}
};

std::string Resource::GetName() const
{
	return name;
}

UID Resource::GetUID() const
{
	return UID;
}

ResourceType Resource::GetType() const
{
	return type;
}

uint32 Resource::GetReferenceCount() const
{
	return referenceCount;
}

std::string Resource::GetAssetsPath() const
{
	return assetsFilePath;
}

std::string Resource::GetLibraryPath() const
{
	return libraryFilePath;
}

std::string Resource::GetStringFromType(ResourceType type)
{
	return resourceTypeToString.at(type);
}