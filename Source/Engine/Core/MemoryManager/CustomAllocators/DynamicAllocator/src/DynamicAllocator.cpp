#include "Engine/Core/MemoryManager/CustomAllocators/DynamicAllocator/include/DynamicAllocator.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"

#include "Engine/Core/Logger/Logger.h"
#include "Engine/Core/Logger/Asserts.h"

uint64 DynamicAllocator::GetMemoryRequirement(uint64 totalSize)
{
    const uint64 freelistReq = Freelist::GetMemoryRequirement(totalSize);
    // Align the start of user memory to 16 bytes so every allocation is 16-byte aligned.
    const uint64 headerSize  = Align16(sizeof(InternalState) + freelistReq);
    return headerSize + totalSize;
}

DynamicAllocator::DynamicAllocator(uint64 totalSize, void* memory)
{
    if (!memory)
    {
        NOUS_FATAL("DynamicAllocator() called with null memory pointer!");
        return;
    }
    if (totalSize == 0)
    {
        NOUS_FATAL("DynamicAllocator() called with totalSize == 0!");
        return;
    }

    state_ = static_cast<InternalState*>(memory);
    new (state_) InternalState();

    state_->totalSize = totalSize;

    char* memPtr = static_cast<char*>(memory);

    // Calculate freelist requirement
    const uint64 freelistReq = Freelist::GetMemoryRequirement(totalSize);

    // Align the header (InternalState + freelist) to 16 bytes so userMemory is
    // 16-byte aligned and every allocation inherits that alignment.
    const uint64 headerSize  = Align16(sizeof(InternalState) + freelistReq);

    state_->freelistMemory = memPtr + sizeof(InternalState);
    state_->userMemory     = memPtr + headerSize;

    // Initialize freelist in pre-allocated space
    state_->freelist = new (&state_->freelistMemory) Freelist(
            totalSize,
            state_->freelistMemory
    );
}

DynamicAllocator::~DynamicAllocator()
{
    if (state_)
    {
        // Leak detection: report any allocations that were never freed.
        {
            std::scoped_lock g(state_->mapMutex);
            if (!state_->allocMap.empty())
            {
                NOUS_WARN("DynamicAllocator::~DynamicAllocator() — %zu allocation(s) never freed (leak):",
                          state_->allocMap.size());
                for (const auto& [ptr, info] : state_->allocMap)
                {
                    NOUS_WARN("  leaked block %p: %llu bytes (aligned: %llu)", ptr, info.rawSize, info.alignedSize);
                }
            }
        }

        state_->freelist->~Freelist();
        nous::engine::memory::ZeroMemory(state_, GetMemoryRequirement(state_->totalSize));
    }
}

void* DynamicAllocator::Allocate(uint64 size)
{
    if (!state_ || size == 0) return nullptr;

    const uint64 aligned = Align16(size);
    uint64 offset = 0;

    if (state_->freelist->Allocate(aligned, &offset))
    {
        void* ptr = static_cast<char*>(state_->userMemory) + offset;
        {   std::scoped_lock g(state_->mapMutex);
            state_->allocMap[ptr] = AllocInfo{ size, aligned };
        }
        return ptr;
    }

    NOUS_ERROR("DynamicAllocator::Allocate() failed. Requested: %llu bytes, Available: %llu bytes",
               size, GetFreeSpace());
    return nullptr;
}

// Sized path — caller provides the size (fast path, no map lookup needed).
bool DynamicAllocator::Free(void* block, uint64 size)
{
    if (!state_ || !block || size == 0) return false;

    char* base = static_cast<char*>(state_->userMemory);
    char* p    = static_cast<char*>(block);
    if (p < base || p >= base + state_->totalSize)
    {
        NOUS_ERROR("DynamicAllocator::Free(size): block %p is out of managed range.", block);
        return false;
    }

    {
        std::scoped_lock g(state_->mapMutex);
        auto it = state_->allocMap.find(block);
        if (it == state_->allocMap.end())
        {
            NOUS_ERROR("DynamicAllocator::Free(size): block %p not in allocMap — possible double-free.", block);
            return false;
        }
        state_->allocMap.erase(it);
    }

    const uint64 aligned = Align16(size);
    const uint64 offset  = static_cast<uint64>(p - base);
    return state_->freelist->Free(aligned, offset);
}

// ✅ New: size-less free (preferred for polymorphic/unknown-size callers)
bool DynamicAllocator::Free(void* block)
{
    if (!state_ || !block) return false;

    uint64 aligned = 0;

    {
        std::scoped_lock g(state_->mapMutex);
        auto it = state_->allocMap.find(block);
        if (it == state_->allocMap.end())
        {
            NOUS_WARN("DynamicAllocator::Free(void*) - unknown block %p", block);
            return false;
        }
        aligned = it->second.alignedSize;
        state_->allocMap.erase(it);
    }

    // Validate block address
    char* userMemStart = static_cast<char*>(state_->userMemory);
    char* userMemEnd   = userMemStart + state_->totalSize;
    char* blockPtr     = static_cast<char*>(block);

    if (blockPtr < userMemStart || blockPtr >= userMemEnd)
    {
        NOUS_ERROR("DynamicAllocator::Free(void*) ERROR: Block out of range");
        return false;
    }

    const uint64 offset = static_cast<uint64>(blockPtr - userMemStart);
    return state_->freelist->Free(aligned, offset);
}

// For MemoryManager stats: return the raw requested size (0 if unknown)
uint64 DynamicAllocator::GetRecordedSize(void* block) const
{
    if (!state_ || !block) return 0;

    std::scoped_lock g(state_->mapMutex);
    auto it = state_->allocMap.find(block);
    return (it != state_->allocMap.end()) ? it->second.rawSize : 0;
}

uint64 DynamicAllocator::GetFreeSpace() const
{
    return state_ ? state_->freelist->FreeSpace() : 0;
}
