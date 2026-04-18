#include <gtest/gtest.h>

#include "Engine/Systems/ECS/Scene/include/Scene.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/ECS/Component/CLight/include/CLight.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/Core/Globals.h"

class t_CLight : public ::testing::Test
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

TEST_F(t_CLight, DefaultType_IsDirectional)
{
    GameObject go = scene->CreateGameObject("Light");
    go.AddComponent<CLight>();
    EXPECT_EQ(go.GetComponent<CLight>().type, LightType::Directional);
}

TEST_F(t_CLight, DefaultColor_IsWhite)
{
    GameObject go = scene->CreateGameObject("Light");
    go.AddComponent<CLight>();
    const glm::vec3 c = go.GetComponent<CLight>().color;
    EXPECT_FLOAT_EQ(c.r, 1.0f);
    EXPECT_FLOAT_EQ(c.g, 1.0f);
    EXPECT_FLOAT_EQ(c.b, 1.0f);
}

TEST_F(t_CLight, DefaultIntensity_Is1)
{
    GameObject go = scene->CreateGameObject("Light");
    go.AddComponent<CLight>();
    EXPECT_FLOAT_EQ(go.GetComponent<CLight>().intensity, 1.0f);
}

TEST_F(t_CLight, DefaultRange_Is10)
{
    GameObject go = scene->CreateGameObject("Light");
    go.AddComponent<CLight>();
    EXPECT_FLOAT_EQ(go.GetComponent<CLight>().range, 10.0f);
}

// =============================================================================
// Field mutation
// =============================================================================

TEST_F(t_CLight, Type_CanBeChangedToPoint)
{
    GameObject go = scene->CreateGameObject("Light");
    go.AddComponent<CLight>();
    go.GetComponent<CLight>().type = LightType::Point;
    EXPECT_EQ(go.GetComponent<CLight>().type, LightType::Point);
}

TEST_F(t_CLight, Color_CanBeChanged)
{
    GameObject go = scene->CreateGameObject("Light");
    go.AddComponent<CLight>();
    go.GetComponent<CLight>().color = glm::vec3(1.0f, 0.0f, 0.0f);
    EXPECT_FLOAT_EQ(go.GetComponent<CLight>().color.r, 1.0f);
    EXPECT_FLOAT_EQ(go.GetComponent<CLight>().color.g, 0.0f);
}

TEST_F(t_CLight, Intensity_CanBeChanged)
{
    GameObject go = scene->CreateGameObject("Light");
    go.AddComponent<CLight>();
    go.GetComponent<CLight>().intensity = 2.5f;
    EXPECT_FLOAT_EQ(go.GetComponent<CLight>().intensity, 2.5f);
}

TEST_F(t_CLight, Range_CanBeChanged)
{
    GameObject go = scene->CreateGameObject("Light");
    go.AddComponent<CLight>();
    go.GetComponent<CLight>().range = 50.0f;
    EXPECT_FLOAT_EQ(go.GetComponent<CLight>().range, 50.0f);
}

// =============================================================================
// ECS mechanics
// =============================================================================

TEST_F(t_CLight, AddComponent_HasComponent_ReturnsTrue)
{
    GameObject go = scene->CreateGameObject("Light");
    go.AddComponent<CLight>();
    EXPECT_TRUE(go.HasComponent<CLight>());
}

TEST_F(t_CLight, GetComponent_ReturnsSameInstance)
{
    GameObject go = scene->CreateGameObject("Light");
    go.AddComponent<CLight>();
    EXPECT_EQ(&go.GetComponent<CLight>(), &go.GetComponent<CLight>());
}

TEST_F(t_CLight, TryGetComponent_NullWhenAbsent)
{
    GameObject go = scene->CreateGameObject("NoLight");
    EXPECT_EQ(go.TryGetComponent<CLight>(), nullptr);
}

TEST_F(t_CLight, RemoveComponent_HasComponentReturnsFalse)
{
    GameObject go = scene->CreateGameObject("Light");
    go.AddComponent<CLight>();
    go.RemoveComponent<CLight>();
    EXPECT_FALSE(go.HasComponent<CLight>());
}

TEST_F(t_CLight, MultipleGameObjects_IndependentCLight)
{
    GameObject a = scene->CreateGameObject("A");
    GameObject b = scene->CreateGameObject("B");
    a.AddComponent<CLight>();
    b.AddComponent<CLight>();
    b.GetComponent<CLight>().intensity = 5.0f;
    EXPECT_FLOAT_EQ(a.GetComponent<CLight>().intensity, 1.0f);
    EXPECT_FLOAT_EQ(b.GetComponent<CLight>().intensity, 5.0f);
}
