#pragma once

#include "Globals.h"

class LinearAllocator 
{
public:

    LinearAllocator();

    LinearAllocator(uint64 capacity, void* preAllocatedMemory = nullptr);
    ~LinearAllocator();

    void Create(uint64 capacity, void* preAllocatedMemory = nullptr);

    void* Allocate(uint64 size);
    void FreeAll();

    // Getters for debugging or inspection
    uint64 GetTotalSize() const;
    uint64 GetAllocatedSize() const;
    uint64 GetRemainingSize() const;

    // Disable copy/move to prevent accidental misuse
    LinearAllocator(const LinearAllocator&) = delete;
    LinearAllocator& operator=(const LinearAllocator&) = delete;
    LinearAllocator(LinearAllocator&&) = delete;
    LinearAllocator& operator=(LinearAllocator&&) = delete;

public:

    uint64 capacity;
    uint64 offset;
    void* memory;
    bool ownsMemory;

};