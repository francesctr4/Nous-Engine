#include "DynamicAllocator.h"

#include "MemoryManager.h"
#include "Logger.h"

 uint64 DynamicAllocator::GetMemoryRequirement(uint64 totalSize)
{
    const uint64 freelistReq = Freelist::GetMemoryRequirement(totalSize);
    return sizeof(InternalState) + freelistReq + totalSize;
}

DynamicAllocator::DynamicAllocator(uint64 totalSize, void* memory) {
    state_ = static_cast<InternalState*>(memory);
    new (state_) InternalState();

    state_->totalSize = totalSize;

    char* memPtr = static_cast<char*>(memory);

    // Calculate freelist requirement
    const uint64 freelistReq = Freelist::GetMemoryRequirement(totalSize);

    // Memory layout correction
    state_->freelistMemory = memPtr + sizeof(InternalState);
    state_->userMemory = memPtr + sizeof(InternalState) + freelistReq;

    // Initialize freelist in pre-allocated space
    state_->freelist = new (&state_->freelistMemory) Freelist(
        totalSize,
        state_->freelistMemory
    );
}

DynamicAllocator::~DynamicAllocator()
{
    if (state_)
    {
        state_->freelist->~Freelist();
        MemoryManager::ZeroMemory(state_, GetMemoryRequirement(state_->totalSize));
    }
}

void* DynamicAllocator::Allocate(uint64 size)
{
    if (!state_ || size == 0) return nullptr;

    uint64 offset;

    if (state_->freelist->Allocate(size, &offset)) 
    {
        return static_cast<char*>(state_->userMemory) + offset;
    }

    // Handle allocation failure
    NOUS_ERROR("DynamicAllocator::Allocate() failed. Requested: %llu bytes, Available: %llu bytes", size, GetFreeSpace());
    NOUS_ASSERT_MSG(size < GetFreeSpace(), "Memory Manager has been initialized with insufficient memory.");

    return nullptr;
}

bool DynamicAllocator::Free(void* block, uint64 size)
{
    if (!state_ || !block || size == 0) return false;

    // Validate block address
    char* userMemStart = static_cast<char*>(state_->userMemory);
    char* userMemEnd = userMemStart + state_->totalSize;
    char* blockPtr = static_cast<char*>(block);

    if (blockPtr < userMemStart || blockPtr >= userMemEnd)
    {
        NOUS_ERROR("DynamicAllocator::Free() ERROR: Block out of range");
        return false;
    }

    const uint64 offset = blockPtr - userMemStart;
    return state_->freelist->Free(size, offset);
}

uint64 DynamicAllocator::GetFreeSpace() const
{
    return state_ ? state_->freelist->FreeSpace() : 0;
}
