#pragma once

#include "Engine/Core/Globals.h"
#include "Engine/EngineExport.h"

#include <mutex>
#include <unordered_map>

class Resource;

// Thread-safe registry of UID -> Resource*. The internal mutex is the canonical
// locking domain for any state that must be kept consistent with the resource
// map (e.g. SubMeshCache's secondary index). Consumers that need compound
// atomic operations should construct a ScopedLock and operate on Map() inside
// that critical section; one-shot callers can use the single-call helpers,
// which lock internally.
class ResourceTable
{
public:
    NOUS_ENGINE_API ResourceTable() = default;
    NOUS_ENGINE_API ~ResourceTable() = default;

    ResourceTable(const ResourceTable&)            = delete;
    ResourceTable& operator=(const ResourceTable&) = delete;

    // RAII lock over the table's mutex. Hold this for any compound
    // read-modify-write cycle that crosses multiple map operations or that
    // must stay consistent with caller-owned state (e.g. a secondary index).
    class ScopedLock
    {
    public:
        NOUS_ENGINE_API explicit ScopedLock(ResourceTable& table)
            : m_lock(table.m_mutex), m_table(&table) {}

        ScopedLock(const ScopedLock&)            = delete;
        ScopedLock& operator=(const ScopedLock&) = delete;
        ScopedLock(ScopedLock&&)                 = delete;

        // Whole-map access while the lock is held. Callers that only need a
        // single read/write should prefer the single-call helpers on the
        // parent table instead.
        [[nodiscard]] std::unordered_map<uint32, Resource*>& Map() noexcept
        {
            return m_table->m_resources;
        }

    private:
        std::scoped_lock<std::mutex> m_lock;
        ResourceTable*               m_table;
    };

    // ---- Single-call helpers (acquire the lock internally) -----------------

    // Returns the pointer for the slot, or nullptr if absent or still loading
    // (placeholder). Does NOT bump the reference count.
    [[nodiscard]] NOUS_ENGINE_API Resource* TryGet(uint32 uid) const;

    // True if the slot has an entry, even a nullptr placeholder.
    [[nodiscard]] NOUS_ENGINE_API bool Contains(uint32 uid) const;

    // Atomic claim — returns true if this thread inserted the slot, false if it
    // was already present. Use to implement load-or-wait patterns.
    NOUS_ENGINE_API bool TryInsert(uint32 uid, Resource* resource);

    // Unconditional set (overwrite or insert). Use to write the final resource
    // pointer into a slot that was previously claimed with TryInsert(nullptr).
    NOUS_ENGINE_API void Set(uint32 uid, Resource* resource);

    // Returns true if an entry was removed.
    NOUS_ENGINE_API bool Erase(uint32 uid);

    // Returns a copy of the map. Safe to call concurrently with any mutation;
    // the snapshot is a moment-in-time view.
    [[nodiscard]] NOUS_ENGINE_API std::unordered_map<uint32, Resource*> Snapshot() const;

    // Drops every entry without locking. Single-threaded shutdown only; using
    // this while other threads can still call the table is a race.
    NOUS_ENGINE_API void Clear() noexcept;

private:
    mutable std::mutex                    m_mutex;
    std::unordered_map<uint32, Resource*> m_resources;
};
