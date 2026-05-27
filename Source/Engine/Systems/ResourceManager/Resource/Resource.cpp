#include "Engine/Systems/ResourceManager/Resource/Resource.h"

#include "Engine/Core/Logger/Asserts.h"
#include "Engine/Systems/ResourceManager/TypeRegistry/TypeRegistry.h"

#include <utility>

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