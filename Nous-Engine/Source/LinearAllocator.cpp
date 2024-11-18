#include "LinearAllocator.h"

#include "MemoryManager.h"

LinearAllocator::LinearAllocator(uint64 capacity, void* preAllocatedMemory) 
    : capacity(capacity), offset(0), memory(preAllocatedMemory), ownsMemory(preAllocatedMemory == nullptr)
{
    if (memory == nullptr) 
    {
        memory = std::malloc(capacity);

        if (memory == nullptr)
        {
            throw std::bad_alloc(); // Handle allocation failure
        }
    }

    std::memset(memory, 0, capacity);
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

// Allocate memory with alignment

void* LinearAllocator::Allocate(uint64 size)
{
    if (offset + size > capacity)
    {
        NOUS_ERROR("LinearAllocator::Allocate - Tried to allocate %lluB, only %lluB remaining.", size, GetRemainingSize());

        return nullptr; // Out of memory
    }

    void* block = static_cast<uint8*>(memory) + offset; // Calculate the block address

    offset += size; // Increment the allocation offset

    return block;  
}

void LinearAllocator::FreeAll()
{
    std::free(memory);

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
