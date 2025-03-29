#pragma once

#include "FreeList.h"

class DynamicAllocator
{
public:

    static uint64 GetMemoryRequirement(uint64 totalSize);

    DynamicAllocator(uint64 totalSize, void* memory);
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
        uint64 totalSize;
        void* freelistMemory;
        void* userMemory;
        Freelist* freelist;

        InternalState() :
            totalSize(0),
            freelistMemory(nullptr),
            userMemory(nullptr),
            freelist(nullptr) {}
    };

    InternalState* state_ = nullptr;
};