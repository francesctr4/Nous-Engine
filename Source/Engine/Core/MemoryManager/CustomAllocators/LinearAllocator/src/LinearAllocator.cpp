#include "Engine/Core/MemoryManager/CustomAllocators/LinearAllocator/include/LinearAllocator.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/Core/Logger/Logger.h"

LinearAllocator::LinearAllocator()
    : capacity(0), offset(0), memory(nullptr), ownsMemory(true) {}

LinearAllocator::LinearAllocator(uint64 capacity, void* preAllocatedMemory)
    : capacity(capacity), offset(0), memory(preAllocatedMemory), ownsMemory(preAllocatedMemory == nullptr)
{
    if (memory == nullptr) 
    {
        memory = MemoryManager::Allocate(capacity, MemoryTag::LINEAR_ALLOCATOR);

        if (memory == nullptr)
        {
            NOUS_ERROR("Allocation Failure");
            throw std::bad_alloc(); // Handle allocation failure
        }
    }

    MemoryManager::ZeroMemory(memory, capacity);
}

LinearAllocator::~LinearAllocator()
{
    if (ownsMemory && memory)
    {
        FreeAll();
    }

    offset = 0;
    capacity = 0;
    memory = nullptr;
    ownsMemory = false;
}

void LinearAllocator::Create(uint64 capacity, void* preAllocatedMemory)
{
    // Guard against re-initialization: free existing owned memory first.
    if (memory != nullptr)
    {
        NOUS_WARN("LinearAllocator::Create() called on an already-initialized allocator. "
                  "Freeing previous allocation to avoid leak.");
        if (ownsMemory)
            MemoryManager::Free(memory, this->capacity, MemoryTag::LINEAR_ALLOCATOR);
        memory = nullptr;
        this->capacity = 0;
        offset = 0;
    }

    this->capacity = capacity;
    this->memory = preAllocatedMemory;
    this->ownsMemory = (preAllocatedMemory == nullptr);
    this->offset = 0;

    if (memory == nullptr)
    {
        memory = MemoryManager::Allocate(capacity, MemoryTag::LINEAR_ALLOCATOR);

        if (memory == nullptr)
        {
            NOUS_ERROR("LinearAllocator::Create() — allocation of %llu bytes failed.", capacity);
            throw std::bad_alloc();
        }
    }

    MemoryManager::ZeroMemory(memory, capacity);
}

void* LinearAllocator::Allocate(uint64 size, uint64 alignment)
{
    if (size == 0)
    {
        NOUS_WARN("LinearAllocator::Allocate() called with size == 0.");
        return nullptr;
    }

    if (!memory)
    {
        NOUS_ERROR("LinearAllocator::Allocate() called on an uninitialized allocator.");
        return nullptr;
    }

    // Round the absolute cursor address up to the requested alignment boundary,
    // then derive the offset from the buffer base.
    // Aligning the relative offset alone is insufficient when the buffer base
    // itself is not aligned to 'alignment'.
    // alignment must be a power of two (not validated here — callers must ensure this).
    const auto cursor       = reinterpret_cast<uintptr_t>(static_cast<uint8*>(memory) + offset);
    const auto aligned      = (cursor + static_cast<uintptr_t>(alignment) - 1)
                              & ~(static_cast<uintptr_t>(alignment) - 1);
    const uint64 alignedOffset = static_cast<uint64>(aligned - reinterpret_cast<uintptr_t>(memory));

    if (alignedOffset + size > capacity)
    {
        NOUS_ERROR("LinearAllocator::Allocate() — requested %llu bytes (aligned offset %llu) but only %llu remain (capacity=%llu).",
                   size, alignedOffset, capacity - alignedOffset, capacity);
        return nullptr;
    }

    void* block = static_cast<uint8*>(memory) + alignedOffset;
    offset = alignedOffset + size;
    return block;
}

void LinearAllocator::Reset()
{
    // Rewind the cursor without touching the backing allocation.
    // Works for both owned and external memory — callers are responsible for
    // re-initialising any objects that were placed in the buffer.
    offset = 0;
}

void LinearAllocator::FreeAll()
{
    if (!ownsMemory)
    {
        // Only reset the cursor — don't free memory we don't own.
        offset = 0;
        return;
    }

    if (memory)
    {
        MemoryManager::Free(memory, capacity, MemoryTag::LINEAR_ALLOCATOR);
        memory = nullptr;
    }
    offset = 0;
}

uint64 LinearAllocator::GetTotalSize() const
{
    return capacity;
}

uint64 LinearAllocator::GetAllocatedSize() const
{
    return offset;
}

uint64 LinearAllocator::GetRemainingSize() const
{
    return capacity - offset;
}
