#pragma once

#include "FreeList.h"

class DynamicAllocator
{
public:

    static uint64 GetMemoryRequirement(uint64 total_size);

    DynamicAllocator(uint64 total_size, void* memory);

    ~DynamicAllocator();

    void* Allocate(uint64 size);

    bool Free(void* block, uint64 size);

    uint64 GetFreeSpace() const;

    // Non-copyable
    DynamicAllocator(const DynamicAllocator&) = delete;
    DynamicAllocator& operator=(const DynamicAllocator&) = delete;

private:

    struct InternalState 
    {
        uint64 total_size;
        void* freelist_memory;
        void* user_memory;
        Freelist* freelist;

        InternalState() :
            total_size(0),
            freelist_memory(nullptr),
            user_memory(nullptr),
            freelist(nullptr) {}
    };

    InternalState* state_ = nullptr;
};