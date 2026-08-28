#pragma once

#include <EngineCore/EngineExport.h>
#include <ResourceManager/Types/ResourceType.h>

#include <mutex>
#include <utility>
#include <vector>
#include <cstddef>

class ResourceBase;

// Thread-safe FIFO of (type, resource) entries used for the two cross-thread
// handoffs in ModuleResourceManager: pending GPU uploads and pending GPU
// releases. Producers (worker threads + main thread) push; the consumer
// (renderer's PreUpdate) drains the whole queue with TakeAll() each frame.
class ResourceQueue
{
public:
    using Entry = std::pair<ResourceType, ResourceBase*>;

    NOUS_ENGINE_API ResourceQueue() = default;
    NOUS_ENGINE_API ~ResourceQueue() = default;

    ResourceQueue(const ResourceQueue&)            = delete;
    ResourceQueue& operator=(const ResourceQueue&) = delete;

    NOUS_ENGINE_API void Push(ResourceType type, ResourceBase* resource);

    // Bulk producer — used for the initial built-in resource priming pass.
    NOUS_ENGINE_API void PushBatch(std::vector<Entry> entries);

    // Atomically removes and returns every queued entry.
    [[nodiscard]] NOUS_ENGINE_API std::vector<Entry> TakeAll();

    // Snapshot of the current queue depth. Exists primarily for tests; runtime
    // code should prefer TakeAll() since the value is racy in any multi-producer
    // scenario.
    [[nodiscard]] NOUS_ENGINE_API size_t Size() const;

    // Drops all queued entries without returning them. Used during full
    // teardown to discard stale entries pointing at already-destroyed resources.
    NOUS_ENGINE_API void Clear();

private:
    mutable std::mutex m_mutex;
    std::vector<Entry> m_entries;
};
