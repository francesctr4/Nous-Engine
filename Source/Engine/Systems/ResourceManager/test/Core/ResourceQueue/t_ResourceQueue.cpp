#include <gtest/gtest.h>

#include <ResourceManager/Core/ResourceQueue.h>
#include <ResourceManager/Core/ResourceBase.h>
#include <ResourceManager/Types/ResourceType.h>

#include <vector>

// ResourceQueue is a thin mutex + FIFO of (type, resource) entries. These are
// contract / regression tests pinning the observable API behaviour: push order
// is preserved, TakeAll swaps-and-clears, PushBatch appends (and no-ops on an
// empty batch). They do NOT prove thread-safety — a single-threaded test can't.
//
// The queue only stores ResourceBase pointers; it never dereferences them, so real
// stack-allocated ResourceBase instances serve as distinct non-null markers.

TEST(ResourceQueue, NewQueueIsEmpty)
{
    ResourceQueue queue;
    EXPECT_EQ(queue.Size(), 0u);
    EXPECT_TRUE(queue.TakeAll().empty());
}

TEST(ResourceQueue, PushIncrementsSize)
{
    ResourceQueue queue;
    ResourceBase a;
    queue.Push(ResourceType::MESH, &a);
    EXPECT_EQ(queue.Size(), 1u);
}

TEST(ResourceQueue, TakeAllReturnsEntriesInPushOrderAndClears)
{
    ResourceQueue queue;
    ResourceBase a;
    ResourceBase b;
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
    ResourceBase a;
    ResourceBase b;
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
    ResourceBase a;
    queue.Push(ResourceType::MESH, &a);

    queue.Clear();
    EXPECT_EQ(queue.Size(), 0u);
    EXPECT_TRUE(queue.TakeAll().empty());
}

// ── The set invariant ─────────────────────────────────────────────────────────
//
// The consumer (ModuleRenderer3D::PreUpdate) calls EvictResource on each drained
// entry, which DELETES the resource. So a second entry for the same pointer is a
// use-after-free: it reads a freed refcount and hands a dead object to the
// importer's Release, where down_cast fails. A resource legitimately reaches zero
// references twice in one frame during nested-prefab refresh, so suppressing the
// duplicate has to happen in the queue.

TEST(ResourceQueue, PushIgnoresAResourceAlreadyQueued)
{
    ResourceQueue queue;
    ResourceBase a;

    queue.Push(ResourceType::MESH, &a);
    queue.Push(ResourceType::MESH, &a);

    EXPECT_EQ(queue.Size(), 1u);
}

// Distinct resources must still both queue -- a dedup keyed on something coarser
// (the type, say) would pass the test above and silently drop real work.
TEST(ResourceQueue, PushKeepsDistinctResourcesOfTheSameType)
{
    ResourceQueue queue;
    ResourceBase a;
    ResourceBase b;

    queue.Push(ResourceType::MESH, &a);
    queue.Push(ResourceType::MESH, &b);

    EXPECT_EQ(queue.Size(), 2u);
}

// The suppression lasts only as long as the entry does. Once drained, the same
// resource may be acquired and released again, and must queue afresh.
TEST(ResourceQueue, PushAcceptsAResourceAgainAfterTakeAll)
{
    ResourceQueue queue;
    ResourceBase a;

    queue.Push(ResourceType::MESH, &a);
    queue.TakeAll();
    queue.Push(ResourceType::MESH, &a);

    EXPECT_EQ(queue.Size(), 1u);
}

TEST(ResourceQueue, PushBatchIgnoresResourcesAlreadyQueued)
{
    ResourceQueue queue;
    ResourceBase a;
    ResourceBase b;

    queue.Push(ResourceType::MESH, &a);
    queue.PushBatch({{ResourceType::MESH, &a}, {ResourceType::TEXTURE, &b}});

    const std::vector<ResourceQueue::Entry> taken = queue.TakeAll();
    ASSERT_EQ(taken.size(), 2u);
    EXPECT_EQ(taken[0].second, &a);
    EXPECT_EQ(taken[1].second, &b);
}

TEST(ResourceQueue, PushBatchIgnoresDuplicatesWithinItsOwnBatch)
{
    ResourceQueue queue;
    ResourceBase a;

    queue.PushBatch({{ResourceType::MESH, &a}, {ResourceType::MESH, &a}});

    EXPECT_EQ(queue.Size(), 1u);
}
