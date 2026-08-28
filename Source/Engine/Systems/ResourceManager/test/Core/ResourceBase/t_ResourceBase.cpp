// Covers ResourceBase: the identity fields, the CPU/GPU state machine, and the
// atomic reference count that every ResourceXxx inherits.
//
// The reference count is the part worth testing. It is std::atomic because
// resources are acquired and released from several job-system workers at once
// (PreloadSceneResourcesAsync) while the editor reads the count; ThreadSanitizer
// flagged it on 2026-08-22 and an undercount frees a resource still in use.
// ConcurrentIncreaseAndDecrease below pins the invariant that survives that.
//
// NOT covered, deliberately: reference-count underflow. DecreaseReferenceCount
// asserts via NOUS_ASSERT_MSG, which aborts the process in a debug build, so a
// test for it would have to be a death test -- slow on Windows and, since the
// assert compiles out of release, testing a different binary than it documents.

#include <gtest/gtest.h>

#include <ResourceManager/Core/ResourceBase.h>
#include <ResourceManager/Types/ResourceType.h>

#include <thread>
#include <vector>

// ---------------------------------------------------------------------------
// Construction / identity
// ---------------------------------------------------------------------------

TEST(t_ResourceBase, DefaultConstructedIsUnloadedAndUnreferenced)
{
    const ResourceBase r;

    EXPECT_EQ(r.GetUID(), 0u);
    EXPECT_EQ(r.GetType(), ResourceType::UNKNOWN);
    EXPECT_EQ(r.GetReferenceCount(), 0u);
    EXPECT_EQ(r.GetState(), ResourceState::UNLOADED);
    EXPECT_FALSE(r.IsLoaded());
    EXPECT_TRUE(r.GetName().empty());
    EXPECT_TRUE(r.GetAssetsPath().empty());
    EXPECT_TRUE(r.GetLibraryPath().empty());
}

TEST(t_ResourceBase, ParameterisedConstructorStoresUIDAndType)
{
    const ResourceBase r(42u, ResourceType::TEXTURE);

    EXPECT_EQ(r.GetUID(), 42u);
    EXPECT_EQ(r.GetType(), ResourceType::TEXTURE);
    // The other fields keep their defaults.
    EXPECT_EQ(r.GetReferenceCount(), 0u);
    EXPECT_EQ(r.GetState(), ResourceState::UNLOADED);
}

TEST(t_ResourceBase, SettersRoundTrip)
{
    ResourceBase r;

    r.SetName("brick_albedo");
    r.SetUID(7u);
    r.SetType(ResourceType::MESH);
    r.SetAssetsPath("Assets/Textures/brick.png");
    r.SetLibraryPath("Library/Textures/7.ntex");

    EXPECT_EQ(r.GetName(), "brick_albedo");
    EXPECT_EQ(r.GetUID(), 7u);
    EXPECT_EQ(r.GetType(), ResourceType::MESH);
    EXPECT_EQ(r.GetAssetsPath(), "Assets/Textures/brick.png");
    EXPECT_EQ(r.GetLibraryPath(), "Library/Textures/7.ntex");
}

TEST(t_ResourceBase, SettersTakeStringViewAndCopyIt)
{
    // The setters take string_view but the members are std::string, so the
    // resource must own its copy -- a borrowed view would dangle here.
    ResourceBase r;
    {
        const std::string temporary = "Assets/Meshes/temp.fbx";
        r.SetAssetsPath(temporary);
    }
    EXPECT_EQ(r.GetAssetsPath(), "Assets/Meshes/temp.fbx");
}

TEST(t_ResourceBase, SetNameOverwritesPreviousValue)
{
    ResourceBase r;
    r.SetName("first");
    r.SetName("second");
    EXPECT_EQ(r.GetName(), "second");
}

// ---------------------------------------------------------------------------
// State machine
// ---------------------------------------------------------------------------

TEST(t_ResourceBase, IsLoadedIsFalseOnlyWhenUnloaded)
{
    ResourceBase r;

    EXPECT_FALSE(r.IsLoaded());

    r.SetState(ResourceState::CPU_READY);
    EXPECT_TRUE(r.IsLoaded());

    r.SetState(ResourceState::GPU_READY);
    EXPECT_TRUE(r.IsLoaded());

    r.SetState(ResourceState::UNLOADED);
    EXPECT_FALSE(r.IsLoaded());
}

TEST(t_ResourceBase, StateTransitionsAreUnconstrained)
{
    // There is no guard rejecting a "backwards" transition: the eviction path
    // deliberately walks GPU_READY -> CPU_READY when the renderer releases a GPU
    // handle but the CPU data is kept. Pinned so a future validity check is a
    // conscious decision rather than a surprise to that path.
    ResourceBase r;
    r.SetState(ResourceState::GPU_READY);
    r.SetState(ResourceState::CPU_READY);
    EXPECT_EQ(r.GetState(), ResourceState::CPU_READY);
}

// ---------------------------------------------------------------------------
// Reference counting
// ---------------------------------------------------------------------------

TEST(t_ResourceBase, IncreaseAndDecreaseAreSymmetric)
{
    ResourceBase r;

    r.IncreaseReferenceCount();
    r.IncreaseReferenceCount();
    EXPECT_EQ(r.GetReferenceCount(), 2u);

    r.DecreaseReferenceCount();
    EXPECT_EQ(r.GetReferenceCount(), 1u);

    r.DecreaseReferenceCount();
    EXPECT_EQ(r.GetReferenceCount(), 0u);
}

TEST(t_ResourceBase, ReferenceCountIsIndependentPerResource)
{
    ResourceBase a;
    ResourceBase b;

    a.IncreaseReferenceCount();

    EXPECT_EQ(a.GetReferenceCount(), 1u);
    EXPECT_EQ(b.GetReferenceCount(), 0u);
}

TEST(t_ResourceBase, ReferenceCountIsIndependentOfState)
{
    // Eviction reads both, and they must not be coupled: a resource can be
    // GPU_READY with zero references (queued for release) or referenced while
    // still UNLOADED (claimed slot, still loading on a worker).
    ResourceBase r;

    r.IncreaseReferenceCount();
    EXPECT_EQ(r.GetState(), ResourceState::UNLOADED);

    r.SetState(ResourceState::GPU_READY);
    r.DecreaseReferenceCount();
    EXPECT_EQ(r.GetReferenceCount(), 0u);
    EXPECT_EQ(r.GetState(), ResourceState::GPU_READY);
}

TEST(t_ResourceBase, ConcurrentIncreaseAndDecrease_NoLostUpdates)
{
    // The regression guard for the TSan finding: as a plain uint32_t this lost
    // increments under concurrent acquisition. Each thread does the same number
    // of increments and decrements, so the count must land back exactly on the
    // starting value -- a lost update shows up as a non-zero residue.
    constexpr int kThreads          = 8;
    constexpr int kIterationsEach   = 10000;

    ResourceBase r;
    // Hold one reference throughout so the count never legitimately reaches zero
    // and the underflow assert cannot fire on an unlucky interleaving.
    r.IncreaseReferenceCount();

    std::vector<std::thread> workers;
    workers.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t)
    {
        workers.emplace_back([&r]
        {
            for (int i = 0; i < kIterationsEach; ++i)
            {
                r.IncreaseReferenceCount();
                r.DecreaseReferenceCount();
            }
        });
    }
    for (auto& w : workers)
        w.join();

    EXPECT_EQ(r.GetReferenceCount(), 1u);
}

TEST(t_ResourceBase, ConcurrentIncreaseOnly_CountsEveryAcquisition)
{
    constexpr int kThreads        = 8;
    constexpr int kIterationsEach = 10000;

    ResourceBase r;

    std::vector<std::thread> workers;
    workers.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t)
    {
        workers.emplace_back([&r]
        {
            for (int i = 0; i < kIterationsEach; ++i)
                r.IncreaseReferenceCount();
        });
    }
    for (auto& w : workers)
        w.join();

    EXPECT_EQ(r.GetReferenceCount(),
              static_cast<uint32_t>(kThreads * kIterationsEach));
}

// ---------------------------------------------------------------------------
// Polymorphism
// ---------------------------------------------------------------------------

TEST(t_ResourceBase, DestructorIsVirtual_DerivedIsDestroyedThroughBasePointer)
{
    // Every ResourceXxx is owned and deleted through a ResourceBase*, so a
    // non-virtual destructor here would leak every derived member silently.
    static bool derivedDestroyed = false;
    derivedDestroyed = false;

    struct DerivedResource final : ResourceBase
    {
        ~DerivedResource() override { derivedDestroyed = true; }
    };

    ResourceBase* r = new DerivedResource();
    delete r;

    EXPECT_TRUE(derivedDestroyed);
}
