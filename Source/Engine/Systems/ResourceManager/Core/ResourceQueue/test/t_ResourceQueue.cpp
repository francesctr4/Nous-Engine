#include <gtest/gtest.h>

#include "Engine/Systems/ResourceManager/Core/ResourceQueue/include/ResourceQueue.h"
#include "Engine/Systems/ResourceManager/Core/Resource/include/Resource.h"
#include "Engine/Systems/ResourceManager/Types/ResourceType.h"

#include <vector>

// ResourceQueue is a thin mutex + FIFO of (type, resource) entries. These are
// contract / regression tests pinning the observable API behaviour: push order
// is preserved, TakeAll swaps-and-clears, PushBatch appends (and no-ops on an
// empty batch). They do NOT prove thread-safety — a single-threaded test can't.
//
// The queue only stores Resource pointers; it never dereferences them, so real
// stack-allocated Resource instances serve as distinct non-null markers.

TEST(ResourceQueue, NewQueueIsEmpty)
{
    ResourceQueue queue;
    EXPECT_EQ(queue.Size(), 0u);
    EXPECT_TRUE(queue.TakeAll().empty());
}

TEST(ResourceQueue, PushIncrementsSize)
{
    ResourceQueue queue;
    Resource a;
    queue.Push(ResourceType::MESH, &a);
    EXPECT_EQ(queue.Size(), 1u);
}

TEST(ResourceQueue, TakeAllReturnsEntriesInPushOrderAndClears)
{
    ResourceQueue queue;
    Resource a;
    Resource b;
    queue.Push(ResourceType::MESH,    &a);
    queue.Push(ResourceType::TEXTURE, &b);

    const std::vector<ResourceQueue::Entry> taken = queue.TakeAll();
    ASSERT_EQ(taken.size(), 2u);
    EXPECT_EQ(taken[0].first,  ResourceType::MESH);
    EXPECT_EQ(taken[0].second, &a);
    EXPECT_EQ(taken[1].first,  ResourceType::TEXTURE);
    EXPECT_EQ(taken[1].second, &b);

    EXPECT_EQ(queue.Size(), 0u); // TakeAll drained the queue
}

TEST(ResourceQueue, TakeAllOnEmptyQueueReturnsEmptyVector)
{
    ResourceQueue queue;
    EXPECT_TRUE(queue.TakeAll().empty());
}

TEST(ResourceQueue, PushBatchAppendsAllEntries)
{
    ResourceQueue queue;
    Resource a;
    Resource b;
    queue.Push(ResourceType::MESH, &a); // pre-existing entry

    std::vector<ResourceQueue::Entry> batch;
    batch.emplace_back(ResourceType::TEXTURE,  &b);
    batch.emplace_back(ResourceType::MATERIAL, nullptr);
    queue.PushBatch(std::move(batch));

    const std::vector<ResourceQueue::Entry> taken = queue.TakeAll();
    ASSERT_EQ(taken.size(), 3u);
    EXPECT_EQ(taken[0].first, ResourceType::MESH);     // original first
    EXPECT_EQ(taken[1].first, ResourceType::TEXTURE);  // batch appended in order
    EXPECT_EQ(taken[2].first, ResourceType::MATERIAL);
}

TEST(ResourceQueue, PushBatchWithEmptyVectorIsNoOp)
{
    ResourceQueue queue;
    queue.PushBatch({});
    EXPECT_EQ(queue.Size(), 0u);
}

TEST(ResourceQueue, ClearDiscardsEntriesWithoutReturningThem)
{
    ResourceQueue queue;
    Resource a;
    queue.Push(ResourceType::MESH, &a);

    queue.Clear();
    EXPECT_EQ(queue.Size(), 0u);
    EXPECT_TRUE(queue.TakeAll().empty());
}
