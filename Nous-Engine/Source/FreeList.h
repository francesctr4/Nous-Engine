#pragma once

#include "Globals.h"

class Freelist
{
public:

#pragma pack(push, 1)
    struct Node 
    {
        uint64 offset;
        uint64 size;
        Node* next;

        Node() : offset(INVALID_ID), size(INVALID_ID), next(nullptr) {}
    };
#pragma pack(pop)

    static uint64 GetMemoryRequirement(uint64 total_size);

    Freelist(uint64 total_size, void* memory);

    ~Freelist();

    bool Allocate(uint64 size, uint64* out_offset);

    bool Free(uint64 size, uint64 offset);

    void Clear();

    uint64 FreeSpace() const;

private:

    struct InternalState 
    {
        uint64 total_size;
        uint64 max_entries;
        Node* head;
        Node* nodes;

        InternalState() : total_size(0), max_entries(0), head(nullptr), nodes(nullptr) {}
    };

    Node* GetNode();

    void ReturnNode(Node* node);

    void MergeWithNext(Node* node);

    InternalState* state_ = nullptr;
};