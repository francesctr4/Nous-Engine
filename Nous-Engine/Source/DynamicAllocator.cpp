#include "DynamicAllocator.h"

 uint64 DynamicAllocator::GetMemoryRequirement(uint64 total_size)
{
    const uint64 freelist_req = Freelist::GetMemoryRequirement(total_size);
    return sizeof(InternalState) + freelist_req + total_size;
}

DynamicAllocator::DynamicAllocator(uint64 total_size, void* memory) {
    state_ = static_cast<InternalState*>(memory);
    new (state_) InternalState();

    state_->total_size = total_size; // Add this line

    char* mem_ptr = static_cast<char*>(memory);

    // Calculate freelist requirement
    const uint64 freelist_req = Freelist::GetMemoryRequirement(total_size);

    // Memory layout correction
    state_->freelist_memory = mem_ptr + sizeof(InternalState);
    state_->user_memory = mem_ptr + sizeof(InternalState) + freelist_req;

    // Initialize freelist in pre-allocated space
    state_->freelist = new Freelist(
        total_size,
        state_->freelist_memory
    );
}

DynamicAllocator::~DynamicAllocator()
{
    if (state_)
    {
        // Explicitly call freelist destructor
        state_->freelist->~Freelist();
        std::memset(state_, 0, GetMemoryRequirement(state_->total_size));
    }
}

void* DynamicAllocator::Allocate(uint64 size)
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

bool DynamicAllocator::Free(void* block, uint64 size)
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

uint64 DynamicAllocator::GetFreeSpace() const
{
    return state_ ? state_->freelist->FreeSpace() : 0;
}
