#ifndef DYNAMICALLOCATOR_H
#define DYNAMICALLOCATOR_H

#include <unordered_map>
#include <mutex>
#include <MemoryManager/FreeList.h>
class DynamicAllocator
{
public:
    static uint64_t GetMemoryRequirement(uint64_t totalSize);

    DynamicAllocator(uint64_t totalSize, void* memory);
    ~DynamicAllocator();

    // Allocation
    void* Allocate(uint64_t size);

    // Free (existing sized path)
    bool Free(void* block, uint64_t size);

    // ✅ New: size-less free (looks up the size internally)
    bool Free(void* block);

    // Optional helper for MemoryManager stats (raw requested size)
    uint64_t GetRecordedSize(void* block) const;

    uint64_t GetFreeSpace() const;

    // Non-copyable
    DynamicAllocator(const DynamicAllocator&) = delete;
    DynamicAllocator& operator=(const DynamicAllocator&) = delete;

private:
    static inline uint64_t Align16(uint64_t s) { return (s + 15ULL) & ~15ULL; }

    struct AllocInfo {
        uint64_t rawSize;     // size requested by caller
        uint64_t alignedSize; // size actually given to freelist
    };

    struct InternalState
    {
        uint64_t   totalSize;
        void*    freelistMemory;
        void*    userMemory;
        Freelist* freelist;

        // Tracks sizes per pointer for size-less free & accurate stats.
        //
        // NOTE (thesis): allocMap uses the system allocator for its internal nodes,
        // meaning those allocations bypass MemoryManager tracking. Using
        // NOUS_STLAllocator here would close that gap, but it creates a circular
        // bootstrap dependency: allocMap insertion → NOUS_STLAllocator::allocate →
        // nous::engine::memory::Allocate → DynamicAllocator::Allocate → allocMap insertion
        // (deadlock on mapMutex, since std::mutex is non-recursive). Solving this
        // requires either a separate node pool, a recursive mutex + re-entrancy guard,
        // or a custom open-addressing hash map that lives entirely within the managed
        // region. Left as a known limitation: map-node overhead is pool memory but
        // is not individually itemised in the tracking stats.
        mutable std::mutex mapMutex;
        std::unordered_map<void*, AllocInfo> allocMap;

        InternalState() :
                totalSize(0),
                freelistMemory(nullptr),
                userMemory(nullptr),
                freelist(nullptr) {}
    };

    InternalState* state_ = nullptr;
};

#endif // DYNAMICALLOCATOR_H