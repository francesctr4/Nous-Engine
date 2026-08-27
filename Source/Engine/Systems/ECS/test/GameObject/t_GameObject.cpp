#include <gtest/gtest.h>

#include <ECS/Scene/Scene.h>
#include <ECS/GameObject.h>
#include <ECS/Component/Types/CTransform.h>
#include <ECS/Component/Types/CCamera.h>
#include <MemoryManager/MemoryManager.h>
#include "Engine/Core/Globals.h"

class t_GameObject : public ::testing::Test
{
protected:
    void SetUp() override
    {
        nous::engine::memory::InitializeMemory(MiB(16));
        scene = NOUS_NEW<Scene>(MemoryTag::SCENE, "TestScene");
    }

    void TearDown() override
    {
        NOUS_DELETE(scene, MemoryTag::SCENE);
        nous::engine::memory::ShutdownMemory();
    }

    Scene* scene = nullptr;
};

// =============================================================================
// Validity
// =============================================================================

TEST_F(t_GameObject, DefaultHandle_IsInvalid)
{
    GameObject go;
    EXPECT_FALSE(go.IsValid());
}

TEST_F(t_GameObject, CreatedGameObject_IsValid)
{
    GameObject go = scene->CreateGameObject("Test");
    EXPECT_TRUE(go.IsValid());
}

TEST_F(t_GameObject, NullHandle_IsNotValid)
{
    GameObject go;
    EXPECT_FALSE(go.IsValid());
}

// =============================================================================
// Name / ID
// =============================================================================

TEST_F(t_GameObject, GetName_ReturnsCreationName)
{
    GameObject go = scene->CreateGameObject("MyObject");
    EXPECT_EQ(go.GetName(), "MyObject");
}

TEST_F(t_GameObject, SetName_UpdatesName)
{
    GameObject go = scene->CreateGameObject("Old");
    go.SetName("New");
    EXPECT_EQ(go.GetName(), "New");
}

TEST_F(t_GameObject, GetID_IsNonZero)
{
    GameObject go = scene->CreateGameObject("IdTest");
    EXPECT_NE(go.GetID(), 0u);
}

TEST_F(t_GameObject, TwoGameObjects_HaveUniqueIDs)
{
    GameObject a = scene->CreateGameObject("A");
    GameObject b = scene->CreateGameObject("B");
    EXPECT_NE(a.GetID(), b.GetID());
}

// =============================================================================
// Equality
// =============================================================================

TEST_F(t_GameObject, SameHandle_Equal)
{
    GameObject go = scene->CreateGameObject("Same");
    GameObject copy = go;
    EXPECT_EQ(go, copy);
}

TEST_F(t_GameObject, DifferentHandles_NotEqual)
{
    GameObject a = scene->CreateGameObject("A");
    GameObject b = scene->CreateGameObject("B");
    EXPECT_NE(a, b);
}

TEST_F(t_GameObject, DefaultHandles_Equal)
{
    GameObject a;
    GameObject b;
    EXPECT_EQ(a, b);
}

// =============================================================================
// Components
// =============================================================================

TEST_F(t_GameObject, NewGO_HasCTransform)
{
    // CreateGameObject always adds CTransform.
    GameObject go = scene->CreateGameObject("WithTransform");
    EXPECT_TRUE(go.HasComponent<CTransform>());
}

TEST_F(t_GameObject, AddComponent_ComponentIsPresent)
{
    GameObject go = scene->CreateGameObject("CamTest");
    go.AddComponent<CCamera>();
    EXPECT_TRUE(go.HasComponent<CCamera>());
}

TEST_F(t_GameObject, GetComponent_ReturnsSameInstance)
{
    GameObject go = scene->CreateGameObject("CamTest2");
    go.AddComponent<CCamera>();
    CCamera& ref1 = go.GetComponent<CCamera>();
    CCamera& ref2 = go.GetComponent<CCamera>();
    EXPECT_EQ(&ref1, &ref2);
}

TEST_F(t_GameObject, RemoveComponent_ComponentIsGone)
{
    GameObject go = scene->CreateGameObject("RemoveTest");
    go.AddComponent<CCamera>();
    ASSERT_TRUE(go.HasComponent<CCamera>());
    go.RemoveComponent<CCamera>();
    EXPECT_FALSE(go.HasComponent<CCamera>());
}

TEST_F(t_GameObject, TryGetComponent_ReturnsNullWhenAbsent)
{
    GameObject go = scene->CreateGameObject("TryGet");
    EXPECT_EQ(go.TryGetComponent<CCamera>(), nullptr);
}

TEST_F(t_GameObject, TryGetComponent_ReturnsPointerWhenPresent)
{
    GameObject go = scene->CreateGameObject("TryGetPresent");
    go.AddComponent<CCamera>();
    EXPECT_NE(go.TryGetComponent<CCamera>(), nullptr);
}

// =============================================================================
// Hierarchy
// =============================================================================

TEST_F(t_GameObject, NewGO_HasNoParent)
{
    GameObject go = scene->CreateGameObject("Root");
    EXPECT_FALSE(go.GetParent().IsValid());
}

TEST_F(t_GameObject, NewGO_HasNoChildren)
{
    GameObject go = scene->CreateGameObject("Leaf");
    EXPECT_TRUE(go.GetChildren().empty());
}

TEST_F(t_GameObject, SetParent_EstablishesRelationship)
{
    GameObject parent = scene->CreateGameObject("Parent");
    GameObject child  = scene->CreateGameObject("Child");
    child.SetParent(parent);

    EXPECT_EQ(child.GetParent(), parent);
    EXPECT_EQ(parent.GetChildren().size(), 1u);
    EXPECT_EQ(parent.GetChildren()[0], child);
}

TEST_F(t_GameObject, CreateGameObject_WithParent_IsChild)
{
    GameObject parent = scene->CreateGameObject("P");
    GameObject child  = scene->CreateGameObject("C", &parent);

    EXPECT_EQ(child.GetParent(), parent);
    EXPECT_EQ(parent.GetChildren().size(), 1u);
}

TEST_F(t_GameObject, DestroyGameObject_RemovesFromScene)
{
    GameObject go = scene->CreateGameObject("ToDestroy");
    const size_t before = scene->GetGameObjectsSnapshot().size();
    scene->DestroyGameObject(go);
    EXPECT_LT(scene->GetGameObjectsSnapshot().size(), before);
}

TEST_F(t_GameObject, IsValid_ReturnsFalseAfterDestroy)
{
    GameObject go = scene->CreateGameObject("GO");
    EXPECT_TRUE(go.IsValid());
    scene->DestroyGameObject(go);
    EXPECT_FALSE(go.IsValid());
}

// =============================================================================
// EnTT view filtering
// =============================================================================

TEST_F(t_GameObject, ViewFilter_OnlyYieldsEntitiesWithBothComponents)
{
    GameObject goA = scene->CreateGameObject("A");
    GameObject goB = scene->CreateGameObject("B");
    goA.AddComponent<CCamera>();
    // goB has no CCamera — must not appear in view<CCamera, CTransform>

    int count = 0;
    scene->GetRegistry().view<CCamera, CTransform>().each([&](auto, auto&, auto&) {
        ++count;
    });
    EXPECT_EQ(count, 1);
}
