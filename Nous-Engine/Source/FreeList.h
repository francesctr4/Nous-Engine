#pragma once

#include "Globals.h"

class Freelist
{
public:

    // The #pragma pack directive is used in C and C++ to control the alignment of structure members in memory.
    // 1 sets the new packing alignment to byte-aligned (no padding between structure members).
    // This means the struct will occupy exactly 24 bytes on a 64-bit system .
    // (8 bytes for offset + 8 bytes for size + 8 bytes for next).
    // Without this pragma, the compiler might insert padding bytes for alignment optimization.

#pragma pack(push, 1)
    struct Node 
    {
        uint64 offset;
        uint64 size;
        Node* next;

        Node() : offset(INVALID_ID), size(INVALID_ID), next(nullptr) {}
    };
#pragma pack(pop)

    static uint64 GetMemoryRequirement(uint64 totalSize);

    Freelist(uint64 totalSize, void* memory);
    ~Freelist();

    bool Allocate(uint64 size, uint64* outOffset);
    bool Free(uint64 size, uint64 offset);

    bool Resize(uint64 newSize, uint64* memoryRequirement, void* newMemory, void** outOldMemory);
    void Clear();

    uint64 FreeSpace() const;

private:

    struct InternalState 
    {
        uint64 totalSize;
        uint64 maxEntries;
        Node* head;
        Node* nodes;

        InternalState() : totalSize(0), maxEntries(0), head(nullptr), nodes(nullptr) {}
    };

    Node* GetNode();

    void ReturnNode(Node* node);
    void MergeWithNext(Node* node);

    InternalState* state_ = nullptr;
};