#include "DynamicAllocator.h"

inline uint64 DynamicAllocator::GetMemoryRequirement(uint64 total_size)
{
    const uint64 freelist_req = Freelist::GetMemoryRequirement(total_size);
    return sizeof(InternalState) + freelist_req + total_size;
}

inline DynamicAllocator::DynamicAllocator(uint64 total_size, void* memory)
{
    if (total_size == 0) {
        // throw or handle error
        return;
    }

    state_ = static_cast<InternalState*>(memory);
    new (state_) InternalState();

    // Memory layout:
    // 1. Internal state (already placed)
    // 2. Freelist memory
    // 3. User memory block
    char* mem_ptr = static_cast<char*>(memory);

    state_->freelist_memory = mem_ptr + sizeof(InternalState);
    state_->user_memory = mem_ptr + sizeof(InternalState) +
        Freelist::GetMemoryRequirement(total_size);

    // Initialize freelist using placement new
    state_->freelist = new (state_->freelist_memory) Freelist(total_size, state_->freelist_memory);

    // Initialize user memory
    std::memset(state_->user_memory, 0, total_size);
    state_->total_size = total_size;
}

inline DynamicAllocator::~DynamicAllocator()
{
    if (state_)
    {
        // Explicitly call freelist destructor
        state_->freelist->~Freelist();
        std::memset(state_, 0, GetMemoryRequirement(state_->total_size));
    }
}

inline void* DynamicAllocator::Allocate(uint64 size)
{
    if (!state_ || size == 0) return nullptr;

    uint64 offset;
    if (state_->freelist->Allocate(size, &offset)) {
        return static_cast<char*>(state_->user_memory) + offset;
    }

    // Handle allocation failure
    // KERROR("Allocation failed. Requested: %lu, Available: %lu", 
    //       size, free_space());
    return nullptr;
}

inline bool DynamicAllocator::Free(void* block, uint64 size)
{
    if (!state_ || !block || size == 0) return false;

    // Validate block address
    char* user_mem_start = static_cast<char*>(state_->user_memory);
    char* user_mem_end = user_mem_start + state_->total_size;
    char* block_ptr = static_cast<char*>(block);

    if (block_ptr < user_mem_start || block_ptr >= user_mem_end) {
        // KERROR("Block out of range");
        return false;
    }

    const uint64 offset = block_ptr - user_mem_start;
    return state_->freelist->Free(size, offset);
}

inline uint64 DynamicAllocator::GetFreeSpace() const
{
    return state_ ? state_->freelist->FreeSpace() : 0;
}
