#include <ResourceManager/Core/ResourceTable.h>

ResourceBase* ResourceTable::TryGet(uint32_t uid) const
{
    std::scoped_lock lock(m_mutex);
    const auto it = m_resources.find(uid);
    if (it == m_resources.end()) return nullptr;
    return it->second;
}

bool ResourceTable::Contains(uint32_t uid) const
{
    std::scoped_lock lock(m_mutex);
    return m_resources.contains(uid);
}

bool ResourceTable::TryInsert(uint32_t uid, ResourceBase* resource)
{
    std::scoped_lock lock(m_mutex);
    if (m_resources.contains(uid)) return false;
    m_resources[uid] = resource;
    return true;
}

void ResourceTable::Set(uint32_t uid, ResourceBase* resource)
{
    std::scoped_lock lock(m_mutex);
    m_resources[uid] = resource;
}

bool ResourceTable::Erase(uint32_t uid)
{
    std::scoped_lock lock(m_mutex);
    return m_resources.erase(uid) > 0;
}

std::unordered_map<uint32_t, ResourceBase*> ResourceTable::Snapshot() const
{
    std::scoped_lock lock(m_mutex);
    return m_resources;
}

void ResourceTable::Clear() noexcept
{
    m_resources.clear();
}
