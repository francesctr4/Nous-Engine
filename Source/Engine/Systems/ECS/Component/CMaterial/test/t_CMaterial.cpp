#include <gtest/gtest.h>

#include "Engine/Systems/ECS/Scene/include/Scene.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/ECS/Component/CMaterial/include/CMaterial.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/Core/Globals.h"

class t_CMaterial : public ::testing::Test
{
protected:
    void SetUp() override
    {
        MemoryManager::InitializeMemory(MiB(16));
        scene = NOUS_NEW<Scene>(MemoryTag::SCENE, "TestScene");
    }

    void TearDown() override
    {
        NOUS_DELETE(scene, MemoryTag::SCENE);
        MemoryManager::ShutdownMemory();
    }

    Scene* scene = nullptr;
};

// =============================================================================
// Default state
// =============================================================================

TEST_F(t_CMaterial, DefaultMaterial_IsNull)
{
    GameObject go = scene->CreateGameObject("Mat");
    go.AddComponent<CMaterial>();
    EXPECT_EQ(go.GetComponent<CMaterial>().material, nullptr);
}

// =============================================================================
// ECS mechanics
// =============================================================================

TEST_F(t_CMaterial, AddComponent_HasComponent_ReturnsTrue)
{
    GameObject go = scene->CreateGameObject("Mat");
    go.AddComponent<CMaterial>();
    EXPECT_TRUE(go.HasComponent<CMaterial>());
}

TEST_F(t_CMaterial, GetComponent_ReturnsSameInstance)
{
    GameObject go = scene->CreateGameObject("Mat");
    go.AddComponent<CMaterial>();
    CMaterial& ref1 = go.GetComponent<CMaterial>();
    CMaterial& ref2 = go.GetComponent<CMaterial>();
    EXPECT_EQ(&ref1, &ref2);
}

TEST_F(t_CMaterial, TryGetComponent_NullWhenAbsent)
{
    GameObject go = scene->CreateGameObject("NoCMat");
    EXPECT_EQ(go.TryGetComponent<CMaterial>(), nullptr);
}

TEST_F(t_CMaterial, TryGetComponent_NonNullWhenPresent)
{
    GameObject go = scene->CreateGameObject("WithCMat");
    go.AddComponent<CMaterial>();
    EXPECT_NE(go.TryGetComponent<CMaterial>(), nullptr);
}

TEST_F(t_CMaterial, RemoveComponent_HasComponentReturnsFalse)
{
    GameObject go = scene->CreateGameObject("RemoveMat");
    go.AddComponent<CMaterial>();
    ASSERT_TRUE(go.HasComponent<CMaterial>());
    go.RemoveComponent<CMaterial>();
    EXPECT_FALSE(go.HasComponent<CMaterial>());
}

TEST_F(t_CMaterial, MultipleGameObjects_IndependentMaterial)
{
    GameObject a = scene->CreateGameObject("A");
    GameObject b = scene->CreateGameObject("B");
    a.AddComponent<CMaterial>();
    b.AddComponent<CMaterial>();
    EXPECT_NE(&a.GetComponent<CMaterial>(), &b.GetComponent<CMaterial>());
}
