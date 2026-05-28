#include "Engine/Systems/ResourceManager/Core/ResourceQueue/include/ResourceQueue.h"

void ResourceQueue::Push(ResourceType type, Resource* resource)
{
    std::scoped_lock lock(m_mutex);
    m_entries.emplace_back(type, resource);
}

void ResourceQueue::PushBatch(std::vector<Entry> entries)
{
    if (entries.empty()) return;
    std::scoped_lock lock(m_mutex);
    m_entries.insert(m_entries.end(),
                     std::make_move_iterator(entries.begin()),
                     std::make_move_iterator(entries.end()));
}

std::vector<ResourceQueue::Entry> ResourceQueue::TakeAll()
{
    std::vector<Entry> result;
    std::scoped_lock lock(m_mutex);
    std::swap(result, m_entries);
    return result;
}

size_t ResourceQueue::Size() const
{
    std::scoped_lock lock(m_mutex);
    return m_entries.size();
}

void ResourceQueue::Clear()
{
    std::scoped_lock lock(m_mutex);
    m_entries.clear();
}
