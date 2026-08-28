#include <ResourceManager/Core/ResourceBase.h>

#include <Logger/Asserts.h>

#include <utility>

ResourceBase::ResourceBase() = default;

ResourceBase::ResourceBase(const uint32_t uID, const ResourceType type)
    : m_uID(uID)
    , m_type(type)
{
}

ResourceBase::~ResourceBase() = default;

void ResourceBase::SetName(const std::string_view name)
{
	this->m_name = name;
}

void ResourceBase::SetUID(uint32_t uid)
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

uint32_t ResourceBase::GetUID() const
{
	return m_uID;
}

ResourceType ResourceBase::GetType() const
{
	return m_type;
}

uint32_t ResourceBase::GetReferenceCount() const
{
	// relaxed: this is an observational read (editor display, diagnostics). It
	// orders nothing, and a caller cannot act on the value safely anyway — the
	// count may change the instant after it is read.
	return m_referenceCount.load(std::memory_order_relaxed);
}

void ResourceBase::IncreaseReferenceCount()
{
	// relaxed: acquiring a reference publishes nothing on its own.
	m_referenceCount.fetch_add(1, std::memory_order_relaxed);
}

void ResourceBase::DecreaseReferenceCount()
{
	// acq_rel: the release half publishes this thread's writes to whichever
	// thread drops the count to zero and tears the resource down; the acquire
	// half makes those writes visible to it.
	//
	// The underflow check reads fetch_sub's return value rather than testing the
	// counter first — a separate load-then-decrement could pass the check on one
	// thread while another already took the count to zero.
	const uint32_t previous = m_referenceCount.fetch_sub(1, std::memory_order_acq_rel);
	NOUS_ASSERT_MSG(previous > 0, "Reference count underflow — double-unload on a resource");
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