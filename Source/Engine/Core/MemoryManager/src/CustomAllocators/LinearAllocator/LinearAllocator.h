#ifndef LINEARALLOCATOR_H
#define LINEARALLOCATOR_H

#include <cstddef>
#include <EngineCore/Globals.h>
#include <EngineCore/EngineExport.h>

class NOUS_ENGINE_API LinearAllocator
{
public:

    LinearAllocator();

    LinearAllocator(uint64 capacity, void* preAllocatedMemory = nullptr);
    ~LinearAllocator();

    void Create(uint64 capacity, void* preAllocatedMemory = nullptr);

    // Allocate 'size' bytes aligned to 'alignment' (must be a power of two).
    // Default alignment is alignof(std::max_align_t) (typically 16 bytes on x64),
    // which satisfies the requirements of any fundamental type.
    void* Allocate(uint64 size, uint64 alignment = alignof(std::max_align_t));

    // Reset rewinds the cursor to 0 without freeing the backing allocation.
    // Suitable for per-frame scratch allocators that are reused each frame.
    void Reset();

    // FreeAll resets the cursor; for owned allocations it also frees the backing block.
    void FreeAll();

    // Getters for debugging or inspection
    uint64 GetTotalSize() const;
    uint64 GetAllocatedSize() const;
    uint64 GetRemainingSize() const;

    // Disable copy/move to prevent accidental misuse
    LinearAllocator(const LinearAllocator&) = delete;
    LinearAllocator& operator=(const LinearAllocator&) = delete;
    LinearAllocator(LinearAllocator&&) = delete;
    LinearAllocator& operator=(LinearAllocator&&) = delete;

private:

    uint64 capacity;
    uint64 offset;
    void*  memory;
    bool   ownsMemory;

};

#endif // LINEARALLOCATOR_H