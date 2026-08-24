#include <gtest/gtest.h>

#include "Engine/Systems/ECS/Scene/include/Scene.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/ECS/Component/Types/CCamera/include/CCamera.h"
#include "Engine/Systems/ECS/Component/Types/CTransform/include/CTransform.h"
#include "Engine/Systems/ECS/test/FakeComponentServices.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/Core/Globals.h"

#include <glm/glm.hpp>
#include <cmath>

class t_CCamera : public ::testing::Test
{
protected:
    void SetUp() override
    {
        nous::engine::memory::InitializeMemory(MiB(16));
        scene = NOUS_NEW<Scene>(MemoryTag::SCENE, "TestScene", nullptr, nullptr, &fakes.services);
    }

    void TearDown() override
    {
        NOUS_DELETE(scene, MemoryTag::SCENE);
        nous::engine::memory::ShutdownMemory();
    }

    // Declared before `scene` so it outlives it — the Scene holds a pointer into it.
    FakeServices fakes;
    Scene*       scene = nullptr;
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

// =============================================================================
// Main-camera sync (requires the ComponentServices seam)
// =============================================================================

TEST_F(t_CCamera, OnUpdate_MainCameraWithNullGameCamera_DoesNotCrash)
{
    // Regression: CCamera.cpp:53 dereferenced scene->GetModuleScene()->gameCamera
    // with no null check on either link. It stayed hidden only because
    // isMainCamera defaults to false, so OnUpdate returned before reaching it.
    // FakeSceneHost::GetGameCamera() returns nullptr, which is a legitimate state
    // (a scene with no game camera assigned).
    GameObject go = scene->CreateGameObject("Camera");
    auto& c = go.AddComponent<CCamera>();
    c.isMainCamera = true;

    c.OnUpdate(0.016f);

    SUCCEED();
}

TEST_F(t_CCamera, OnUpdate_MainCamera_ReadsAspectFromHost)
{
    fakes.host.aspect = 2.0f;

    GameObject go = scene->CreateGameObject("Camera");
    auto& c = go.AddComponent<CCamera>();
    c.isMainCamera = true;

    c.OnUpdate(0.016f);

    // Aspect must be written even when there is no game camera to push it into.
    EXPECT_FLOAT_EQ(c.aspectRatio, 2.0f);
}

TEST_F(t_CCamera, OnUpdate_NotMainCamera_LeavesAspectUntouched)
{
    fakes.host.aspect = 2.0f;

    GameObject go = scene->CreateGameObject("Camera");
    auto& c = go.AddComponent<CCamera>();
    c.isMainCamera  = false;
    c.aspectRatio   = 1.0f;

    c.OnUpdate(0.016f);

    EXPECT_FLOAT_EQ(c.aspectRatio, 1.0f);
}

TEST_F(t_CCamera, OnUpdate_UnwiredScene_DoesNotCrash)
{
    Scene* bare = NOUS_NEW<Scene>(MemoryTag::SCENE, "Bare");
    GameObject go = bare->CreateGameObject("Camera");
    auto& c = go.AddComponent<CCamera>();
    c.isMainCamera = true;

    c.OnUpdate(0.016f);

    NOUS_DELETE(bare, MemoryTag::SCENE);
    SUCCEED();
}
