#include "Engine/Systems/ResourceManager/Core/ResourceBase/include/ResourceBase.h"

#include "Engine/Core/Logger/Asserts.h"

#include <utility>

ResourceBase::ResourceBase() = default;

ResourceBase::ResourceBase(const uint32 uID, const ResourceType type)
    : m_uID(uID)
    , m_type(type)
{
}

ResourceBase::~ResourceBase() = default;

void ResourceBase::SetName(const std::string_view name)
{
	this->m_name = name;
}

void ResourceBase::SetUID(uint32 uid)
{
	this->m_uID = uid;
}

void ResourceBase::SetType(ResourceType rType)
{
	this->m_type = rType;
}

void ResourceBase::SetAssetsPath(const std::string_view assetsFilePath)
{
	this->m_assetsFilePath = assetsFilePath;
}

void ResourceBase::SetLibraryPath(const std::string_view libraryFilePath)
{
	this->m_libraryFilePath = libraryFilePath;
}

std::string ResourceBase::GetName() const
{
	return m_name;
}

uint32 ResourceBase::GetUID() const
{
	return m_uID;
}

ResourceType ResourceBase::GetType() const
{
	return m_type;
}

uint32 ResourceBase::GetReferenceCount() const
{
	return m_referenceCount;
}

void ResourceBase::IncreaseReferenceCount()
{
	m_referenceCount++;
}

void ResourceBase::DecreaseReferenceCount()
{
	NOUS_ASSERT_MSG(m_referenceCount > 0, "Reference count underflow — double-unload on a resource");
	m_referenceCount--;
}

bool ResourceBase::IsLoaded() const
{
	return m_state != ResourceState::UNLOADED;
}

ResourceState ResourceBase::GetState() const
{
	return m_state;
}

void ResourceBase::SetState(const ResourceState newState)
{
	m_state = newState;
}

std::string ResourceBase::GetAssetsPath() const
{
	return m_assetsFilePath;
}

std::string ResourceBase::GetLibraryPath() const
{
	return m_libraryFilePath;
}