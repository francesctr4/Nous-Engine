#include "LinearAllocator.h"

#include "MemoryManager.h"

LinearAllocator::LinearAllocator()
    : capacity(0), offset(0), memory(nullptr), ownsMemory(true) {}

LinearAllocator::LinearAllocator(uint64 capacity, void* preAllocatedMemory)
    : capacity(capacity), offset(0), memory(preAllocatedMemory), ownsMemory(preAllocatedMemory == nullptr)
{
    if (memory == nullptr) 
    {
        memory = MemoryManager::Allocate(capacity, MemoryManager::MemoryTag::LINEAR_ALLOCATOR);

        if (memory == nullptr)
        {
            NOUS_ERROR("%s() - Allocation Failure", __FUNCTION__);
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
    this->capacity = capacity;
    this->offset = offset;
    this->memory = preAllocatedMemory;
    this->ownsMemory = (preAllocatedMemory == nullptr);

    if (memory == nullptr)
    {
        memory = MemoryManager::Allocate(capacity, MemoryManager::MemoryTag::LINEAR_ALLOCATOR);

        if (memory == nullptr)
        {
            throw std::bad_alloc(); // Handle allocation failure
        }
    }

    MemoryManager::ZeroMemory(memory, capacity);
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
    MemoryManager::Free(memory, capacity, MemoryManager::MemoryTag::LINEAR_ALLOCATOR);

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
