#include <ResourceManager/Core/ResourceQueue.h>
#include <cstddef>

// Caller must hold m_mutex. Linear because these queues hold a handful of entries
// per frame; a set would cost more bookkeeping than it saves.
bool ResourceQueue::ContainsUnlocked(const ResourceBase* resource) const
{
    for (const Entry& entry : m_entries)
        if (entry.second == resource) return true;
    return false;
}

void ResourceQueue::Push(ResourceType type, ResourceBase* resource)
{
    std::scoped_lock lock(m_mutex);

    // THE QUEUE IS A SET, NOT A MULTISET, and that is a correctness requirement
    // rather than tidiness. A resource can cross zero references more than once
    // before a drain -- PrefabManager::ReloadPrefabInstance destroys a subtree
    // (release), rebuilds it (re-acquire, resolving to the SAME resident pointer
    // because eviction is deferred), and an outer nested prefab's refresh then
    // destroys it again (release). The drain in ModuleRenderer3D::PreUpdate calls
    // EvictResource on the first entry, which DELETES the resource -- so a second
    // entry for the same pointer is a use-after-free, landing as a failed
    // down_cast inside the importer's Release.
    if (ContainsUnlocked(resource)) return;

    m_entries.emplace_back(type, resource);
}

void ResourceQueue::PushBatch(std::vector<Entry> entries)
{
    if (entries.empty()) return;
    std::scoped_lock lock(m_mutex);

    // Same set invariant as Push.
    for (Entry& entry : entries)
    {
        if (ContainsUnlocked(entry.second)) continue;
        m_entries.emplace_back(std::move(entry));
    }
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
