#include "FreeList.h"

uint64 Freelist::GetMemoryRequirement(uint64 total_size) 
{
    const uint64 max_entries = total_size / (sizeof(void*) * sizeof(Node));
    return sizeof(InternalState) + (sizeof(Node) * max_entries);
}

Freelist::Freelist(uint64 total_size, void* memory)
{
    state_ = reinterpret_cast<InternalState*>(memory);
    new (state_) InternalState();

    state_->nodes = reinterpret_cast<Node*>(reinterpret_cast<char*>(memory) + sizeof(InternalState));
    state_->max_entries = total_size / (sizeof(void*) * sizeof(Node));
    state_->total_size = total_size;

    // Initialize first node
    state_->head = &state_->nodes[0];
    state_->head->offset = 0;
    state_->head->size = total_size;
    state_->head->next = nullptr;

    // Initialize remaining nodes
    for (uint64 i = 1; i < state_->max_entries; ++i) {
        state_->nodes[i] = Node();
    }
}

Freelist::~Freelist() 
{
    if (state_) {
        // Clear memory as per original implementation
        std::memset(state_, 0, GetMemoryRequirement(state_->total_size));
    }
}

bool Freelist::Allocate(uint64 size, uint64* out_offset)
{
    if (!out_offset || !state_) return false;

    Node* current = state_->head;
    Node* previous = nullptr;

    while (current) {
        if (current->size == size) {
            *out_offset = current->offset;
            if (previous) {
                previous->next = current->next;
                ReturnNode(current);
            }
            else {
                Node* old_head = state_->head;
                state_->head = current->next;
                ReturnNode(old_head);
            }
            return true;
        }
        else if (current->size > size) {
            *out_offset = current->offset;
            current->size -= size;
            current->offset += size;
            return true;
        }

        previous = current;
        current = current->next;
    }

    // Log warning about insufficient space
    return false;
}

bool Freelist::Free(uint64 size, uint64 offset) 
{
    if (!state_ || size == 0) return false;

    if (!state_->head) {
        Node* new_node = GetNode();
        new_node->offset = offset;
        new_node->size = size;
        state_->head = new_node;
        return true;
    }

    Node* current = state_->head;
    Node* previous = nullptr;

    while (current) {
        if (current->offset == offset) {
            current->size += size;
            MergeWithNext(current);
            return true;
        }
        else if (current->offset > offset) {
            Node* new_node = GetNode();
            new_node->offset = offset;
            new_node->size = size;

            if (previous) {
                previous->next = new_node;
            }
            else {
                state_->head = new_node;
            }
            new_node->next = current;

            MergeWithNext(new_node);
            if (previous) MergeWithNext(previous);

            return true;
        }

        previous = current;
        current = current->next;
    }

    // Log warning about possible corruption
    return false;
}

void Freelist::Clear()
{
    if (!state_) return;

    for (uint64 i = 1; i < state_->max_entries; ++i) {
        state_->nodes[i] = Node();
    }

    state_->head->offset = 0;
    state_->head->size = state_->total_size;
    state_->head->next = nullptr;
}

uint64 Freelist::FreeSpace() const
{
    if (!state_) return 0;

    uint64 total = 0;
    Node* current = state_->head;
    while (current) {
        total += current->size;
        current = current->next;
    }
    return total;
}

Freelist::Node* Freelist::GetNode()
{
    for (uint64 i = 1; i < state_->max_entries; ++i) {
        if (state_->nodes[i].offset == INVALID_ID) {
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
    if (node->next && (node->offset + node->size == node->next->offset)) {
        node->size += node->next->size;
        Node* rubbish = node->next;
        node->next = rubbish->next;
        ReturnNode(rubbish);
    }
}
