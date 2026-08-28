#ifndef LINEARALLOCATOR_H
#define LINEARALLOCATOR_H

#include <cstddef>
#include <cstdint>
class LinearAllocator
{
public:

    LinearAllocator();

    LinearAllocator(uint64_t capacity, void* preAllocatedMemory = nullptr);
    ~LinearAllocator();

    void Create(uint64_t capacity, void* preAllocatedMemory = nullptr);

    // Allocate 'size' bytes aligned to 'alignment' (must be a power of two).
    // Default alignment is alignof(std::max_align_t) (typically 16 bytes on x64),
    // which satisfies the requirements of any fundamental type.
    void* Allocate(uint64_t size, uint64_t alignment = alignof(std::max_align_t));

    // Reset rewinds the cursor to 0 without freeing the backing allocation.
    // Suitable for per-frame scratch allocators that are reused each frame.
    void Reset();

    // FreeAll resets the cursor; for owned allocations it also frees the backing block.
    void FreeAll();

    // Getters for debugging or inspection
    uint64_t GetTotalSize() const;
    uint64_t GetAllocatedSize() const;
    uint64_t GetRemainingSize() const;

    // Disable copy/move to prevent accidental misuse
    LinearAllocator(const LinearAllocator&) = delete;
    LinearAllocator& operator=(const LinearAllocator&) = delete;
    LinearAllocator(LinearAllocator&&) = delete;
    LinearAllocator& operator=(LinearAllocator&&) = delete;

private:

    uint64_t capacity;
    uint64_t offset;
    void*  memory;
    bool   ownsMemory;

};

#endif // LINEARALLOCATOR_H