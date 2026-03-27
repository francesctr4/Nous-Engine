#include "Engine/Systems/ResourceManager/Resource/Resource.h"
#include <unordered_map>

#pragma region UTILITY MAPS

static const std::unordered_map<ResourceType, std::string> resourceTypeToLibraryExtension
{
	{ResourceType::MESH, "nmesh"},
	{ResourceType::MATERIAL, "nmat"},
	{ResourceType::TEXTURE, "png"},
	{ResourceType::SHADER, ""}     // Shaders are stored as a directory of .spv stage files
};

static const std::unordered_map<std::string_view, ResourceType> extensionToResourceType
{
	{"fbx", ResourceType::MESH},
	{"obj", ResourceType::MESH},
	{"nmesh", ResourceType::MESH},

	{"nmat", ResourceType::MATERIAL},

	{"png", ResourceType::TEXTURE},

	{"glsl", ResourceType::SHADER},
	{"spv", ResourceType::SHADER},
};

static const std::unordered_map<ResourceType, std::string> resourceTypeToAssetsFolder
{
	{ResourceType::MESH, "Assets\\Meshes\\"},
	{ResourceType::MATERIAL, "Assets\\Materials\\"},
	{ResourceType::TEXTURE, "Assets\\Textures\\"},
	{ResourceType::SHADER, "Assets\\Shaders\\"},
};

static const std::unordered_map<ResourceType, std::string> resourceTypeToLibraryFolder
{
	{ResourceType::MESH, "Library\\Meshes\\"},
	{ResourceType::MATERIAL, "Library\\Materials\\"},
	{ResourceType::TEXTURE, "Library\\Textures\\"},
	{ResourceType::SHADER, "Library\\Shaders\\"},
};

#pragma endregion

Resource::Resource()
{
	this->type = ResourceType::UNKNOWN;
	this->uID = 0;
	this->referenceCount = 0;
	this->state = ResourceState::UNLOADED;
	this->valid = false;
}

Resource::Resource(UID uID, ResourceType type)
{
	this->type = type;
	this->uID = uID;
	this->referenceCount = 0;
	this->state = ResourceState::UNLOADED;
	this->valid = false;
}

Resource::~Resource()
{
	this->name.clear();
	this->uID = 0;
	this->type = ResourceType::UNKNOWN;
	this->referenceCount = 0;

	this->assetsFilePath.clear();
	this->libraryFilePath.clear();

	this->valid = false;
}

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

bool Resource::IsValid() const
{
	return valid;
}

void Resource::Validate()
{
	valid = true;
}

void Resource::Invalidate()
{
	valid = false;
}

ResourceState Resource::GetState() const
{
	return state;
}

void Resource::SetState(ResourceState newState)
{
	state = newState;
}

std::string Resource::GetAssetsPath() const
{
	return assetsFilePath;
}

std::string Resource::GetLibraryPath() const
{
	return libraryFilePath;
}

int16 Resource::GetIndexFromType(const ResourceType& type)
{
	return static_cast<int16>(type);
}

std::string Resource::GetLibraryExtensionFromType(ResourceType type)
{
	return resourceTypeToLibraryExtension.at(type);
}

ResourceType Resource::GetTypeFromExtension(const std::string& extension)
{
	std::string_view normalizedExtension = (extension[0] == '.') ? std::string_view(extension).substr(1) : extension;

	auto it = extensionToResourceType.find(normalizedExtension);

	return (it != extensionToResourceType.end()) ? it->second : ResourceType::UNKNOWN;
}

std::string Resource::GetAssetsDirectoryFromType(ResourceType type)
{
	return resourceTypeToAssetsFolder.at(type);
}

std::string Resource::GetLibraryDirectoryFromType(ResourceType type)
{
	return resourceTypeToLibraryFolder.at(type);
}