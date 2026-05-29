#include <gtest/gtest.h>

#include "Engine/Systems/ResourceManager/Core/ResourceTable/include/ResourceTable.h"
#include "Engine/Systems/ResourceManager/Core/Resource/include/Resource.h"

#include <unordered_map>

// ResourceTable is a thin mutex + UID->Resource* map. These are contract /
// regression tests: they pin the observable API behaviour (placeholder vs
// resolved slots, atomic-claim semantics of TryInsert, Snapshot being a copy).
// They do NOT attempt to prove thread-safety — a single-threaded test cannot.
//
// The table only stores and returns Resource pointers; it never dereferences
// them, so real stack-allocated Resource instances serve as distinct non-null
// markers and a literal nullptr serves as the "still loading" placeholder.

// ---- TryInsert (atomic claim) ----------------------------------------------

TEST(ResourceTable, TryInsertNewSlotReturnsTrue)
{
    ResourceTable table;
    Resource res;
    EXPECT_TRUE(table.TryInsert(1, &res));
    EXPECT_EQ(table.TryGet(1), &res);
}

TEST(ResourceTable, TryInsertExistingSlotReturnsFalseAndDoesNotOverwrite)
{
    ResourceTable table;
    Resource first;
    Resource second;
    ASSERT_TRUE(table.TryInsert(1, &first));

    EXPECT_FALSE(table.TryInsert(1, &second));
    EXPECT_EQ(table.TryGet(1), &first); // original entry preserved
}

TEST(ResourceTable, TryInsertNullPlaceholderStillClaimsSlot)
{
    ResourceTable table;
    Resource res;
    ASSERT_TRUE(table.TryInsert(1, nullptr)); // claim with placeholder
    EXPECT_FALSE(table.TryInsert(1, &res));   // slot is taken, even though null
}

// ---- TryGet vs Contains (placeholder distinction) --------------------------

TEST(ResourceTable, TryGetAbsentSlotReturnsNull)
{
    ResourceTable table;
    EXPECT_EQ(table.TryGet(42), nullptr);
    EXPECT_FALSE(table.Contains(42));
}

TEST(ResourceTable, PlaceholderSlotContainsTrueButTryGetReturnsNull)
{
    ResourceTable table;
    ASSERT_TRUE(table.TryInsert(1, nullptr));
    EXPECT_TRUE(table.Contains(1));     // the slot exists ...
    EXPECT_EQ(table.TryGet(1), nullptr); // ... but has no resolved resource yet
}

// ---- Set (unconditional write) ---------------------------------------------

TEST(ResourceTable, SetResolvesPreviouslyClaimedPlaceholder)
{
    ResourceTable table;
    Resource res;
    ASSERT_TRUE(table.TryInsert(1, nullptr)); // claim
    table.Set(1, &res);                       // resolve
    EXPECT_EQ(table.TryGet(1), &res);
}

TEST(ResourceTable, SetInsertsWhenSlotAbsent)
{
    ResourceTable table;
    Resource res;
    table.Set(7, &res);
    EXPECT_EQ(table.TryGet(7), &res);
}

TEST(ResourceTable, SetOverwritesExistingResource)
{
    ResourceTable table;
    Resource first;
    Resource second;
    table.Set(1, &first);
    table.Set(1, &second);
    EXPECT_EQ(table.TryGet(1), &second);
}

// ---- Erase ------------------------------------------------------------------

TEST(ResourceTable, EraseReturnsTrueWhenPresentFalseWhenAbsent)
{
    ResourceTable table;
    Resource res;
    table.Set(1, &res);

    EXPECT_TRUE(table.Erase(1));
    EXPECT_FALSE(table.Contains(1));
    EXPECT_FALSE(table.Erase(1)); // already gone
}

// ---- Snapshot ---------------------------------------------------------------

TEST(ResourceTable, SnapshotReflectsCurrentContents)
{
    ResourceTable table;
    Resource a;
    Resource b;
    table.Set(1, &a);
    table.Set(2, &b);

    const std::unordered_map<uint32, Resource*> snap = table.Snapshot();
    ASSERT_EQ(snap.size(), 2u);
    EXPECT_EQ(snap.at(1), &a);
    EXPECT_EQ(snap.at(2), &b);
}

TEST(ResourceTable, SnapshotIsACopyAndDoesNotTrackLaterMutations)
{
    ResourceTable table;
    Resource a;
    table.Set(1, &a);

    const std::unordered_map<uint32, Resource*> snap = table.Snapshot();
    table.Erase(1); // mutate after snapshotting

    EXPECT_EQ(snap.size(), 1u);     // snapshot unchanged
    EXPECT_FALSE(table.Contains(1)); // live table reflects the erase
}

// ---- ScopedLock -------------------------------------------------------------

TEST(ResourceTable, ScopedLockExposesSameUnderlyingMap)
{
    ResourceTable table;
    Resource a;
    {
        ResourceTable::ScopedLock lock(table);
        lock.Map()[1] = &a; // direct compound mutation under the lock
    }
    EXPECT_EQ(table.TryGet(1), &a);
}

// ---- Clear ------------------------------------------------------------------

TEST(ResourceTable, ClearEmptiesTheTable)
{
    ResourceTable table;
    Resource a;
    Resource b;
    table.Set(1, &a);
    table.Set(2, &b);

    table.Clear();
    EXPECT_FALSE(table.Contains(1));
    EXPECT_FALSE(table.Contains(2));
    EXPECT_TRUE(table.Snapshot().empty());
}
