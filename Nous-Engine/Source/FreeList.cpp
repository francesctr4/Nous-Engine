#include "FreeList.h"

#include "MemoryManager.h"
#include "Logger.h"

uint64 Freelist::GetMemoryRequirement(uint64 totalSize) 
{
    const uint64 maxEntries = totalSize / (sizeof(void*) * sizeof(Node));
    return sizeof(InternalState) + (sizeof(Node) * maxEntries);
}

Freelist::Freelist(uint64 totalSize, void* memory)
{
    state_ = reinterpret_cast<InternalState*>(memory);
    new (state_) InternalState();

    state_->nodes = reinterpret_cast<Node*>(reinterpret_cast<char*>(memory) + sizeof(InternalState));
    state_->maxEntries = totalSize / (sizeof(void*) * sizeof(Node));
    state_->totalSize = totalSize;

    // Initialize first node
    state_->head = &state_->nodes[0];
    state_->head->offset = 0;
    state_->head->size = totalSize;
    state_->head->next = nullptr;

    // Initialize remaining nodes
    for (uint64 i = 1; i < state_->maxEntries; ++i) 
    {
        state_->nodes[i] = Node();
    }
}

Freelist::~Freelist() 
{
    if (state_) 
    {
        MemoryManager::ZeroMemory(state_, GetMemoryRequirement(state_->totalSize));
    }
}

bool Freelist::Allocate(uint64 size, uint64* outOffset)
{
    if (!outOffset || !state_) return false;

    Node* current = state_->head;
    Node* previous = nullptr;

    while (current) 
    {
        if (current->size == size) 
        {
            *outOffset = current->offset;
            if (previous) 
            {
                previous->next = current->next;
                ReturnNode(current);
            }
            else 
            {
                Node* old_head = state_->head;
                state_->head = current->next;
                ReturnNode(old_head);
            }

            return true;

        }
        else if (current->size > size) 
        {
            *outOffset = current->offset;
            current->size -= size;
            current->offset += size;
            return true;
        }

        previous = current;
        current = current->next;
    }

    // Log warning about insufficient space
    NOUS_WARN("Freelist::Allocate(). WARNING: Insufficient Space");

    return false;
}

bool Freelist::Free(uint64 size, uint64 offset) 
{
    if (!state_ || size == 0) return false;

    if (!state_->head) 
    {
        Node* newNode = GetNode();
        newNode->offset = offset;
        newNode->size = size;
        state_->head = newNode;
        return true;
    }

    Node* current = state_->head;
    Node* previous = nullptr;

    while (current) 
    {
        if (current->offset == offset) 
        {
            current->size += size;
            MergeWithNext(current);

            return true;
        }
        else if (current->offset > offset) 
        {
            Node* new_node = GetNode();
            new_node->offset = offset;
            new_node->size = size;

            if (previous) 
            {
                previous->next = new_node;
            }
            else 
            {
                state_->head = new_node;
            }

            new_node->next = current;

            MergeWithNext(new_node);

            if (previous) 
            {
                MergeWithNext(previous);
            }
                
            return true;
        }

        previous = current;
        current = current->next;
    }

    // Log warning about possible corruption
    NOUS_WARN("Freelist::Free(). WARNING: Possible Memory Corruption.");

    return false;
}

void Freelist::Clear()
{
    if (!state_) return;

    for (uint64 i = 1; i < state_->maxEntries; ++i) 
    {
        state_->nodes[i] = Node();
    }

    state_->head->offset = 0;
    state_->head->size = state_->totalSize;
    state_->head->next = nullptr;
}

uint64 Freelist::FreeSpace() const
{
    if (!state_) return 0;

    uint64 total = 0;
    Node* current = state_->head;

    while (current) 
    {
        total += current->size;
        current = current->next;
    }

    return total;
}

bool Freelist::Resize(uint64 newSize, uint64* memoryRequirement, void* newMemory, void** outOldMemory) 
{
    if (!memoryRequirement || state_->totalSize > newSize) 
    {
        return false;
    }

    // Calculate new memory requirements
    uint64_t maxEntries = newSize / (sizeof(void*) * sizeof(Node));
    *memoryRequirement = sizeof(InternalState) + (sizeof(Node) * maxEntries);

    // If just querying memory requirement, return early
    if (!newMemory) 
    {
        return true;
    }

    // Prepare old state and calculate size difference
    InternalState* oldState = state_;
    uint64_t sizeDiff = newSize - oldState->totalSize;

    // Set output for old memory block (caller must free this)
    *outOldMemory = reinterpret_cast<void*>(oldState);

    // Initialize new memory block
    MemoryManager::ZeroMemory(newMemory, *memoryRequirement);
    state_ = reinterpret_cast<InternalState*>(newMemory);
    state_->nodes = reinterpret_cast<Node*>(reinterpret_cast<char*>(newMemory) + sizeof(InternalState));
    state_->maxEntries = maxEntries;
    state_->totalSize = newSize;

    // Invalidate all nodes except the first
    for (uint64_t i = 1; i < state_->maxEntries; ++i) 
    {
        state_->nodes[i].offset = INVALID_ID;
        state_->nodes[i].size = INVALID_ID;
        state_->nodes[i].next = nullptr;
    }

    state_->head = &state_->nodes[0];

    // Copy old nodes to new memory
    if (!oldState->head) 
    {
        // Entire old list was allocated: create new head at the expanded space
        state_->head->offset = oldState->totalSize;
        state_->head->size = sizeDiff;
        state_->head->next = nullptr;
    }
    else 
    {
        Node* newCurrent = state_->head;
        Node* oldCurrent = oldState->head;

        while (oldCurrent) 
        {
            // Replicate the node in the new memory
            newCurrent->offset = oldCurrent->offset;
            newCurrent->size = oldCurrent->size;
            newCurrent->next = nullptr;

            // Handle expansion at the end of the old list
            if (!oldCurrent->next && (oldCurrent->offset + oldCurrent->size == oldState->totalSize)) 
            {
                newCurrent->size += sizeDiff;
            }

            // Move to next node if available
            if (oldCurrent->next) 
            {
                newCurrent->next = GetNode();

                newCurrent = newCurrent->next;
                oldCurrent = oldCurrent->next;
            }
            else 
            {
                break;
            }
        }
    }

    return true;
}

Freelist::Node* Freelist::GetNode()
{
    for (uint64 i = 1; i < state_->maxEntries; ++i) 
    {
        if (state_->nodes[i].offset == INVALID_ID) 
        {
            state_->nodes[i].offset = 0;
            state_->nodes[i].size = 0;

            return &state_->nodes[i];
        }
    }

    return nullptr;
}

void Freelist::ReturnNode(Node* node) 
{
    node->offset = INVALID_ID;
    node->size = INVALID_ID;
    node->next = nullptr;
}

void Freelist::MergeWithNext(Node* node) 
{
    if (node->next && (node->offset + node->size == node->next->offset)) 
    {
        node->size += node->next->size;

        Node* rubbish = node->next;
        node->next = rubbish->next;
        ReturnNode(rubbish);
    }
}
