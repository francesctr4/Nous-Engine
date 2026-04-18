#include <gtest/gtest.h>

#include "Engine/Systems/ECS/Scene/include/Scene.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/ECS/Component/CCamera/include/CCamera.h"
#include "Engine/Systems/ECS/Component/CTransform/include/CTransform.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/Core/Globals.h"

#include <glm/glm.hpp>
#include <cmath>

class t_CCamera : public ::testing::Test
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

TEST_F(t_CCamera, DefaultFov_Is60)
{
    GameObject go = scene->CreateGameObject("Cam");
    go.AddComponent<CCamera>();
    EXPECT_FLOAT_EQ(go.GetComponent<CCamera>().fov, 60.0f);
}

TEST_F(t_CCamera, DefaultNearPlane_Is01)
{
    GameObject go = scene->CreateGameObject("Cam");
    go.AddComponent<CCamera>();
    EXPECT_FLOAT_EQ(go.GetComponent<CCamera>().nearPlane, 0.1f);
}

TEST_F(t_CCamera, DefaultFarPlane_Is1000)
{
    GameObject go = scene->CreateGameObject("Cam");
    go.AddComponent<CCamera>();
    EXPECT_FLOAT_EQ(go.GetComponent<CCamera>().farPlane, 1000.0f);
}

TEST_F(t_CCamera, DefaultAspectRatio_Is16Over9)
{
    GameObject go = scene->CreateGameObject("Cam");
    go.AddComponent<CCamera>();
    EXPECT_NEAR(go.GetComponent<CCamera>().aspectRatio, 16.0f / 9.0f, 1e-4f);
}

TEST_F(t_CCamera, DefaultIsMainCamera_IsTrue)
{
    GameObject go = scene->CreateGameObject("Cam");
    go.AddComponent<CCamera>();
    EXPECT_TRUE(go.GetComponent<CCamera>().isMainCamera);
}

// =============================================================================
// Projection matrix
// =============================================================================

TEST_F(t_CCamera, GetProjectionMatrix_IsNotZero)
{
    GameObject go = scene->CreateGameObject("Cam");
    go.AddComponent<CCamera>();
    const glm::mat4 proj = go.GetComponent<CCamera>().GetProjectionMatrix();
    float sum = 0.0f;
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            sum += std::fabs(proj[c][r]);
    EXPECT_GT(sum, 0.0f);
}

TEST_F(t_CCamera, GetProjectionMatrix_ChangesWithFov)
{
    GameObject go = scene->CreateGameObject("Cam");
    go.AddComponent<CCamera>();
    const glm::mat4 proj60 = go.GetComponent<CCamera>().GetProjectionMatrix();
    go.GetComponent<CCamera>().fov = 90.0f;
    const glm::mat4 proj90 = go.GetComponent<CCamera>().GetProjectionMatrix();
    EXPECT_NE(proj60, proj90);
}

// =============================================================================
// View matrix
// =============================================================================

TEST_F(t_CCamera, GetViewMatrix_IsNotAllZero)
{
    GameObject go = scene->CreateGameObject("Cam");
    go.AddComponent<CCamera>();
    const glm::mat4 view = go.GetComponent<CCamera>().GetViewMatrix();
    float sum = 0.0f;
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            sum += std::fabs(view[c][r]);
    EXPECT_GT(sum, 0.0f);
}

// =============================================================================
// ECS mechanics
// =============================================================================

TEST_F(t_CCamera, AddComponent_HasComponent_ReturnsTrue)
{
    GameObject go = scene->CreateGameObject("Cam");
    go.AddComponent<CCamera>();
    EXPECT_TRUE(go.HasComponent<CCamera>());
}

TEST_F(t_CCamera, GetComponent_ReturnsSameInstance)
{
    GameObject go = scene->CreateGameObject("Cam");
    go.AddComponent<CCamera>();
    EXPECT_EQ(&go.GetComponent<CCamera>(), &go.GetComponent<CCamera>());
}

TEST_F(t_CCamera, TryGetComponent_NullWhenAbsent)
{
    GameObject go = scene->CreateGameObject("NoCam");
    EXPECT_EQ(go.TryGetComponent<CCamera>(), nullptr);
}

TEST_F(t_CCamera, TryGetComponent_NonNullWhenPresent)
{
    GameObject go = scene->CreateGameObject("Cam");
    go.AddComponent<CCamera>();
    EXPECT_NE(go.TryGetComponent<CCamera>(), nullptr);
}

TEST_F(t_CCamera, RemoveComponent_HasComponentReturnsFalse)
{
    GameObject go = scene->CreateGameObject("Cam");
    go.AddComponent<CCamera>();
    go.RemoveComponent<CCamera>();
    EXPECT_FALSE(go.HasComponent<CCamera>());
}

TEST_F(t_CCamera, MultipleGameObjects_IndependentCCamera)
{
    GameObject a = scene->CreateGameObject("A");
    GameObject b = scene->CreateGameObject("B");
    a.AddComponent<CCamera>();
    b.AddComponent<CCamera>();
    b.GetComponent<CCamera>().fov = 120.0f;
    EXPECT_FLOAT_EQ(a.GetComponent<CCamera>().fov, 60.0f);
    EXPECT_FLOAT_EQ(b.GetComponent<CCamera>().fov, 120.0f);
}
