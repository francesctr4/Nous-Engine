#include "Engine/Systems/ResourceManager/Core/ResourceTable/include/ResourceTable.h"

Resource* ResourceTable::TryGet(uint32 uid) const
{
    std::scoped_lock lock(m_mutex);
    const auto it = m_resources.find(uid);
    if (it == m_resources.end()) return nullptr;
    return it->second;
}

bool ResourceTable::Contains(uint32 uid) const
{
    std::scoped_lock lock(m_mutex);
    return m_resources.contains(uid);
}

bool ResourceTable::TryInsert(uint32 uid, Resource* resource)
{
    std::scoped_lock lock(m_mutex);
    if (m_resources.contains(uid)) return false;
    m_resources[uid] = resource;
    return true;
}

void ResourceTable::Set(uint32 uid, Resource* resource)
{
    std::scoped_lock lock(m_mutex);
    m_resources[uid] = resource;
}

bool ResourceTable::Erase(uint32 uid)
{
    std::scoped_lock lock(m_mutex);
    return m_resources.erase(uid) > 0;
}

std::unordered_map<uint32, Resource*> ResourceTable::Snapshot() const
{
    std::scoped_lock lock(m_mutex);
    return m_resources;
}

void ResourceTable::Clear() noexcept
{
    m_resources.clear();
}
