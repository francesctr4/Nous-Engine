#include "LinearAllocator_Test.h"

#include "TestManager.h"
#include "Expect.h"

#include "Globals.h"
#include "LinearAllocator.h"

bool LinearAllocator_should_create_and_destroy() 
{
    LinearAllocator alloc(sizeof(uint64));

    EXPECT_SHOULD_NOT_BE(nullptr, alloc.memory);
    EXPECT_SHOULD_BE(sizeof(uint64), alloc.capacity);
    EXPECT_SHOULD_BE(0, alloc.offset);

    alloc.~LinearAllocator();

    EXPECT_SHOULD_BE(nullptr, alloc.memory);
    EXPECT_SHOULD_BE(0, alloc.capacity);
    EXPECT_SHOULD_BE(0, alloc.offset);

    return true;
}

bool LinearAllocator_single_allocation_all_space() 
{
    LinearAllocator alloc(sizeof(uint64));

    // Single allocation.
    void* block = alloc.Allocate(sizeof(uint64));

    // Validate it
    EXPECT_SHOULD_NOT_BE(nullptr, block);
    EXPECT_SHOULD_BE(sizeof(uint64), alloc.offset);

    alloc.~LinearAllocator();

    EXPECT_SHOULD_BE(nullptr, alloc.memory);
    EXPECT_SHOULD_BE(0, alloc.capacity);
    EXPECT_SHOULD_BE(0, alloc.offset);

    return true;
}

bool LinearAllocator_multi_allocation_all_space() 
{
    uint64 maxAllocs = 1024;

    LinearAllocator alloc(sizeof(uint64) * maxAllocs);

    // Multiple allocations - full.
    void* block;

    for (uint64 i = 0; i < maxAllocs; ++i) 
    {
        block = alloc.Allocate(sizeof(uint64));
        // Validate it
        EXPECT_SHOULD_NOT_BE(0, block);
        EXPECT_SHOULD_BE(sizeof(uint64) * (i + 1), alloc.offset);
    }

    alloc.~LinearAllocator();

    EXPECT_SHOULD_BE(nullptr, alloc.memory);
    EXPECT_SHOULD_BE(0, alloc.capacity);
    EXPECT_SHOULD_BE(0, alloc.offset);

    return true;
}

bool LinearAllocator_multi_allocation_over_allocate() 
{
    uint64 maxAllocs = 3;

    LinearAllocator alloc(sizeof(uint64) * maxAllocs);

    // Multiple allocations - full.

    void* block;

    for (uint64 i = 0; i < maxAllocs; ++i)
    {
        block = alloc.Allocate(sizeof(uint64));
        // Validate it
        EXPECT_SHOULD_NOT_BE(0, block);
        EXPECT_SHOULD_BE(sizeof(uint64) * (i + 1), alloc.offset);
    }

    NOUS_DEBUG("Note: The following error is intentionally caused by this test.");

    // Ask for one more allocation. Should error and return 0.
    block = alloc.Allocate(sizeof(uint64));

    // Validate it - offset should be unchanged.
    EXPECT_SHOULD_BE(0, block);
    EXPECT_SHOULD_BE(sizeof(uint64) * (maxAllocs), alloc.offset);

    alloc.~LinearAllocator();

    EXPECT_SHOULD_BE(nullptr, alloc.memory);
    EXPECT_SHOULD_BE(0, alloc.capacity);
    EXPECT_SHOULD_BE(0, alloc.offset);

    return true;
}

bool LinearAllocator_multi_allocation_all_space_then_free() 
{
    uint64 maxAllocs = 1024;

    LinearAllocator alloc(sizeof(uint64) * maxAllocs);

    // Multiple allocations - full.
    void* block;

    for (uint64 i = 0; i < maxAllocs; ++i) 
    {
        block = alloc.Allocate(sizeof(uint64));
        // Validate it
        EXPECT_SHOULD_NOT_BE(0, block);
        EXPECT_SHOULD_BE(sizeof(uint64) * (i + 1), alloc.offset);
    }

    // Validate that pointer is reset.
    alloc.FreeAll();

    EXPECT_SHOULD_BE(0, alloc.offset);

    alloc.~LinearAllocator();

    EXPECT_SHOULD_BE(nullptr, alloc.memory);
    EXPECT_SHOULD_BE(0, alloc.capacity);
    EXPECT_SHOULD_BE(0, alloc.offset);

    return true;
}

void LinearAllocatorRegisterTests(TestManager* testManager)
{
    testManager->RegisterTest("Linear allocator should create and destroy", LinearAllocator_should_create_and_destroy);
    testManager->RegisterTest("Linear allocator single alloc for all space", LinearAllocator_single_allocation_all_space);
    testManager->RegisterTest("Linear allocator multi alloc for all space", LinearAllocator_multi_allocation_all_space);
    testManager->RegisterTest("Linear allocator try over allocate", LinearAllocator_multi_allocation_over_allocate);
    testManager->RegisterTest("Linear allocator offset should be 0 after free_all", LinearAllocator_multi_allocation_all_space_then_free);
}