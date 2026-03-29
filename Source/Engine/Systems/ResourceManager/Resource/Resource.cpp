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
	this->m_type = ResourceType::UNKNOWN;
	this->m_uID = 0;
	this->m_referenceCount = 0;
	this->m_state = ResourceState::UNLOADED;
	this->m_valid = false;
}

Resource::Resource(const UID uID, const ResourceType type)
{
	this->m_type = type;
	this->m_uID = uID;
	this->m_referenceCount = 0;
	this->m_state = ResourceState::UNLOADED;
	this->m_valid = false;
}

Resource::~Resource()
{
	this->m_name.clear();
	this->m_uID = 0;
	this->m_type = ResourceType::UNKNOWN;
	this->m_referenceCount = 0;

	this->m_assetsFilePath.clear();
	this->m_libraryFilePath.clear();

	this->m_valid = false;
}

void Resource::SetName(const std::string_view name)
{
	this->m_name = name;
}

void Resource::SetUID(const UID& uid)
{
	this->m_uID = uid;
}

void Resource::SetType(const ResourceType& rType)
{
	this->m_type = rType;
}

void Resource::SetAssetsPath(const std::string_view assetsFilePath)
{
	this->m_assetsFilePath = assetsFilePath;
}

void Resource::SetLibraryPath(const std::string_view libraryFilePath)
{
	this->m_libraryFilePath = libraryFilePath;
}

std::string Resource::GetName() const
{
	return m_name;
}

UID Resource::GetUID() const
{
	return m_uID;
}

ResourceType Resource::GetType() const
{
	return m_type;
}

uint32 Resource::GetReferenceCount() const
{
	return m_referenceCount;
}

void Resource::IncreaseReferenceCount()
{
	m_referenceCount++;
}

void Resource::DecreaseReferenceCount()
{
	m_referenceCount--;
}

bool Resource::IsValid() const
{
	return m_valid;
}

void Resource::Validate()
{
	m_valid = true;
}

void Resource::Invalidate()
{
	m_valid = false;
}

ResourceState Resource::GetState() const
{
	return m_state;
}

void Resource::SetState(const ResourceState newState)
{
	m_state = newState;
}

std::string Resource::GetAssetsPath() const
{
	return m_assetsFilePath;
}

std::string Resource::GetLibraryPath() const
{
	return m_libraryFilePath;
}

int16 Resource::GetIndexFromType(const ResourceType& type)
{
	return static_cast<int16>(std::to_underlying(type));
}

std::string Resource::GetLibraryExtensionFromType(const ResourceType type)
{
	return resourceTypeToLibraryExtension.at(type);
}

ResourceType Resource::GetTypeFromExtension(const std::string& extension)
{
	const std::string_view normalizedExtension = extension[0] == '.' ? std::string_view(extension).substr(1) : extension;

	const auto it = extensionToResourceType.find(normalizedExtension);

	return it != extensionToResourceType.end() ? it->second : ResourceType::UNKNOWN;
}

std::string Resource::GetAssetsDirectoryFromType(const ResourceType type)
{
	return resourceTypeToAssetsFolder.at(type);
}

std::string Resource::GetLibraryDirectoryFromType(const ResourceType type)
{
	return resourceTypeToLibraryFolder.at(type);
}