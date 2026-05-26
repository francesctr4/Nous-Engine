#include "Engine/Systems/ResourceManager/Resource/Resource.h"

#include "Engine/Core/Logger/Asserts.h"

#include <unordered_map>
#include <utility>

#pragma region UTILITY MAPS

static const std::unordered_map<ResourceType, std::string> resourceTypeToLibraryExtension
{
	{ResourceType::MESH, "nmesh"},
	{ResourceType::MATERIAL, "nmat"},
	{ResourceType::TEXTURE, "png"},
	{ResourceType::SHADER, ""},    // Shaders are stored as a directory of .spv stage files
	{ResourceType::AUDIO,  ""}     // Audio preserves source extension (wav/ogg); ImporterAudio builds the path
};

static const std::unordered_map<std::string_view, ResourceType> extensionToResourceType
{
	{"fbx",  ResourceType::MESH},
	{"obj",  ResourceType::MESH},
	{"glb",  ResourceType::MESH},
	{"gltf", ResourceType::MESH},
	{"nmesh", ResourceType::MESH},

	{"nmat", ResourceType::MATERIAL},

	{"png",  ResourceType::TEXTURE},
	{"jpg",  ResourceType::TEXTURE},
	{"jpeg", ResourceType::TEXTURE},
	{"tga",  ResourceType::TEXTURE},

	{"glsl", ResourceType::SHADER},
	{"spv",  ResourceType::SHADER},

	{"wav",  ResourceType::AUDIO},
	{"ogg",  ResourceType::AUDIO},
};

static const std::unordered_map<ResourceType, std::string> resourceTypeToLibraryFolder
{
	{ResourceType::MESH, "Library/Meshes/"},
	{ResourceType::MATERIAL, "Library/Materials/"},
	{ResourceType::TEXTURE, "Library/Textures/"},
	{ResourceType::SHADER, "Library/Shaders/"},
	{ResourceType::AUDIO, "Library/Audio/"},
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

Resource::Resource(const uint32 uID, const ResourceType type)
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

void Resource::SetUID(uint32 uid)
{
	this->m_uID = uid;
}

void Resource::SetType(ResourceType rType)
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

uint32 Resource::GetUID() const
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
	NOUS_ASSERT_MSG(m_referenceCount > 0, "Reference count underflow — double-unload on a resource");
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

std::string Resource::GetLibraryDirectoryFromType(const ResourceType type)
{
	return resourceTypeToLibraryFolder.at(type);
}