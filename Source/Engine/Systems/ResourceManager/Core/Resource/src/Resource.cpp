#include "Engine/Systems/ResourceManager/Core/Resource/include/Resource.h"

#include "Engine/Core/Logger/Asserts.h"
#include "Engine/Systems/ResourceManager/Core/TypeRegistry/include/TypeRegistry.h"

#include <utility>

Resource::Resource() = default;

Resource::Resource(const uint32 uID, const ResourceType type)
    : m_uID(uID)
    , m_type(type)
{
}

Resource::~Resource() = default;

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

bool Resource::IsLoaded() const
{
	return m_state != ResourceState::UNLOADED;
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
	const TypeDescriptor* d = GetTypeRegistry().Get(type);
	NOUS_ASSERT_MSG(d != nullptr, "GetLibraryExtensionFromType: type not in registry");
	return d->libraryFixedExtension;
}

ResourceType Resource::GetTypeFromExtension(const std::string& extension)
{
	return GetTypeRegistry().TypeFromExtension(extension);
}

std::string Resource::GetLibraryDirectoryFromType(const ResourceType type)
{
	const TypeDescriptor* d = GetTypeRegistry().Get(type);
	NOUS_ASSERT_MSG(d != nullptr, "GetLibraryDirectoryFromType: type not in registry");
	return d->libraryFolder;
}