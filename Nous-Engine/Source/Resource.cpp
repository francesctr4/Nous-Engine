#include "Resource.h"

#pragma region UTILITY MAPS

std::map<ResourceType, std::string> resourceTypeToString
{
	{ResourceType::MESH, "nmesh"},
	{ResourceType::MATERIAL, "nmat"},
	{ResourceType::TEXTURE, "png"}
};

std::map<ResourceType, std::string> resourceTypeToAssetsFolder
{
	{ResourceType::MESH, "Assets\\Meshes\\"},
	{ResourceType::MATERIAL, "Assets\\Materials\\"},
	{ResourceType::TEXTURE, "Assets\\Textures\\"},
};

std::map<ResourceType, std::string> resourceTypeToLibraryFolder
{
	{ResourceType::MESH, "Library\\Meshes\\"},
	{ResourceType::MATERIAL, "Library\\Materials\\"},
	{ResourceType::TEXTURE, "Library\\Textures\\"},
};

#pragma endregion

void Resource::SetName(const std::string& name)
{
	this->name = name;
}

void Resource::SetUID(const UID& uid)
{
	this->uID = uid;
}

void Resource::SetType(const ResourceType& rType)
{
	this->type = rType;
}

void Resource::SetAssetsPath(const std::string& assetsFilePath)
{
	this->assetsFilePath = assetsFilePath;
}

void Resource::SetLibraryPath(const std::string& libraryFilePath)
{
	this->libraryFilePath = libraryFilePath;
}

std::string Resource::GetName() const
{
	return name;
}

UID Resource::GetUID() const
{
	return uID;
}

ResourceType Resource::GetType() const
{
	return type;
}

uint32 Resource::GetReferenceCount() const
{
	return referenceCount;
}

void Resource::IncreaseReferenceCount()
{
	referenceCount++;
}

void Resource::DecreaseReferenceCount()
{
	referenceCount--;
}

std::string Resource::GetAssetsPath() const
{
	return assetsFilePath;
}

std::string Resource::GetLibraryPath() const
{
	return libraryFilePath;
}

bool Resource::IsLoadedOnMemory() const
{
	return isLoaded;
}

std::string Resource::GetStringFromType(ResourceType type)
{
	return resourceTypeToString.at(type);
}

ResourceType Resource::GetTypeFromExtension(const std::string& extension)
{
	ResourceType tmpType = ResourceType::UNKNOWN;

	// Remove the leading dot, if present.
	std::string normalizedExtension = extension;

	if (!extension.empty() && extension[0] == '.')
	{
		normalizedExtension = extension.substr(1);
	}

	for (const auto& pair : resourceTypeToString)
	{
		if (pair.second == normalizedExtension)
		{
			tmpType = pair.first;
			break;
		}
	}

	return tmpType;
}

std::string Resource::GetAssetsDirectoryFromType(ResourceType type)
{
	return resourceTypeToAssetsFolder.at(type);
}

std::string Resource::GetLibraryDirectoryFromType(ResourceType type)
{
	return resourceTypeToLibraryFolder.at(type);
}
