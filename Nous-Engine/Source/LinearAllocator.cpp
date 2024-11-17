#include "LinearAllocator.h"

#include "MemoryManager.h"

LinearAllocator::LinearAllocator(uint64 capacity, void* preAllocatedMemory = nullptr) 
    : capacity(capacity), offset(0), memory(preAllocatedMemory), ownsMemory(preAllocatedMemory == nullptr)
{
    if (memory == nullptr) 
    {
        memory = NOUS_NEW_ARRAY<uint8>(capacity, MemoryManager::MemoryTag::LINEAR_ALLOCATOR);
    }

    MemoryManager::ZeroMemory(memory, capacity);
}

LinearAllocator::~LinearAllocator()
{
    if (ownsMemory && memory)
    {
        FreeAll();
    }
}

// Allocate memory with alignment

void* LinearAllocator::Allocate(uint64 size)
{
    if (offset + size > capacity)
    {
        NOUS_ERROR("linear_allocator_allocate - Tried to allocate %lluB, only %lluB remaining.", size, GetRemainingSize());

        return nullptr; // Out of memory
    }

    void* block = static_cast<uint8*>(memory) + offset; // Calculate the block address

    offset += size; // Increment the allocation offset

    return block;  
}

void LinearAllocator::FreeAll()
{
    NOUS_DELETE_ARRAY(memory, capacity, MemoryManager::MemoryTag::LINEAR_ALLOCATOR);

    memory = nullptr;
    offset = 0;
}

inline uint64 LinearAllocator::GetTotalSize() const 
{ 
    return capacity; 
}

inline uint64 LinearAllocator::GetAllocatedSize() const 
{ 
    return offset; 
}

inline uint64 LinearAllocator::GetRemainingSize() const 
{ 
    return capacity - offset; 
}
