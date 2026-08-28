#include <EngineCore/InvalidID.h>
#ifndef FREELIST_H
#define FREELIST_H

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
        uint64_t offset;
        uint64_t size;
        Node* next;

        Node() : offset(INVALID_ID), size(INVALID_ID), next(nullptr) {}
    };
#pragma pack(pop)

    static uint64_t GetMemoryRequirement(uint64_t totalSize);

    Freelist(uint64_t totalSize, void* memory);
    ~Freelist();

    bool Allocate(uint64_t size, uint64_t* outOffset);
    bool Free(uint64_t size, uint64_t offset);

    bool Resize(uint64_t newSize, uint64_t* memoryRequirement, void* newMemory, void** outOldMemory);
    void Clear();

    uint64_t FreeSpace() const;

private:

    struct InternalState 
    {
        uint64_t totalSize;
        uint64_t maxEntries;
        Node* head;
        Node* nodes;

        InternalState() : totalSize(0), maxEntries(0), head(nullptr), nodes(nullptr) {}
    };

    Node* GetNode();

    void ReturnNode(Node* node);
    void MergeWithNext(Node* node);

    InternalState* state_ = nullptr;
};

#endif // FREELIST_H