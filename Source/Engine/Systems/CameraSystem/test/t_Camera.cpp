#include <gtest/gtest.h>

#include <CameraSystem/Camera.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

// =============================================================================
// Position
// =============================================================================

TEST(t_Camera, SetPos_GetPos_Roundtrip)
{
    Camera cam;
    cam.SetPos(1.f, 2.f, 3.f);
    const glm::vec3 p = cam.GetPos();
    EXPECT_FLOAT_EQ(p.x, 1.f);
    EXPECT_FLOAT_EQ(p.y, 2.f);
    EXPECT_FLOAT_EQ(p.z, 3.f);
}

TEST(t_Camera, SetPos_Vec3_Roundtrip)
{
    Camera cam;
    cam.SetPos(glm::vec3(4.f, 5.f, 6.f));
    const glm::vec3 p = cam.GetPos();
    EXPECT_FLOAT_EQ(p.x, 4.f);
    EXPECT_FLOAT_EQ(p.y, 5.f);
    EXPECT_FLOAT_EQ(p.z, 6.f);
}

TEST(t_Camera, UpdatePos_ChangesPosition)
{
    Camera cam;
    cam.SetPos(0.f, 0.f, 0.f);
    cam.UpdatePos(glm::vec3(7.f, 0.f, 0.f));
    EXPECT_FLOAT_EQ(cam.GetPos().x, 7.f);
}

// =============================================================================
// Orientation vectors
// =============================================================================

TEST(t_Camera, SetFront_GetFront_Roundtrip)
{
    Camera cam;
    cam.SetFront(glm::vec3(0.f, 0.f, -1.f));
    EXPECT_FLOAT_EQ(cam.GetFront().z, -1.f);
}

TEST(t_Camera, SetUp_GetUp_Roundtrip)
{
    Camera cam;
    cam.SetUp(glm::vec3(0.f, 1.f, 0.f));
    EXPECT_FLOAT_EQ(cam.GetUp().y, 1.f);
}

TEST(t_Camera, GetRight_IsPerpendicularToFrontAndUp)
{
    Camera cam;
    cam.SetFront(glm::vec3(0.f, 0.f, -1.f));
    cam.SetUp(glm::vec3(0.f, 1.f, 0.f));
    const glm::vec3 right = cam.GetRight();
    EXPECT_NEAR(glm::dot(right, cam.GetFront()), 0.f, 1e-5f);
    EXPECT_NEAR(glm::dot(right, cam.GetUp()),    0.f, 1e-5f);
}

// =============================================================================
// FOV
// =============================================================================

TEST(t_Camera, SetVerticalFOV_GetVerticalFOV_Roundtrip)
{
    Camera cam;
    cam.SetVerticalFOV(glm::radians(60.f));
    EXPECT_NEAR(cam.GetVerticalFOV(), glm::radians(60.f), 1e-5f);
}

TEST(t_Camera, SetBothFOV_AffectsVerticalFOV)
{
    Camera cam;
    cam.SetBothFOV(glm::radians(45.f));
    EXPECT_GT(cam.GetVerticalFOV(), 0.f);
}

TEST(t_Camera, SetHorizontalFOV_Roundtrip)
{
    Camera cam;
    cam.SetHorizontalFOV(glm::radians(90.f));
    EXPECT_NEAR(cam.GetHorizontalFOV(), glm::radians(90.f), 1e-5f);
}

// =============================================================================
// Near / Far planes
// =============================================================================

TEST(t_Camera, SetNearPlane_Roundtrip)
{
    Camera cam;
    cam.SetNearPlane(0.1f);
    EXPECT_FLOAT_EQ(cam.GetNearPlane(), 0.1f);
}

TEST(t_Camera, SetFarPlane_Roundtrip)
{
    Camera cam;
    cam.SetFarPlane(1000.f);
    EXPECT_FLOAT_EQ(cam.GetFarPlane(), 1000.f);
}

// =============================================================================
// Aspect ratio
// =============================================================================

TEST(t_Camera, SetAspectRatio_Roundtrip)
{
    Camera cam;
    cam.SetAspectRatio(16.f / 9.f);
    EXPECT_NEAR(cam.GetAspectRatio(), 16.f / 9.f, 1e-5f);
}

// =============================================================================
// Matrices
// =============================================================================

TEST(t_Camera, GetProjectionMatrix_IsNotZero)
{
    Camera cam;
    cam.SetVerticalFOV(glm::radians(60.f));
    cam.SetAspectRatio(16.f / 9.f);
    cam.SetNearPlane(0.1f);
    cam.SetFarPlane(100.f);

    const glm::mat4 proj = cam.GetProjectionMatrix();
    float sum = 0.f;
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            sum += std::abs(proj[c][r]);
    EXPECT_GT(sum, 0.f);
}

TEST(t_Camera, GetViewMatrix_ChangesWithPosition)
{
    Camera cam;
    cam.SetFront(glm::vec3(0.f, 0.f, -1.f));
    cam.SetUp(glm::vec3(0.f, 1.f, 0.f));

    cam.SetPos(0.f, 0.f, 0.f);
    const glm::mat4 v1 = cam.GetViewMatrix();

    cam.SetPos(5.f, 0.f, 0.f);
    const glm::mat4 v2 = cam.GetViewMatrix();

    EXPECT_NE(v1[3][0], v2[3][0]);
}

TEST(t_Camera, GetProjectionMatrix_ChangesWithFOV)
{
    Camera cam;
    cam.SetAspectRatio(1.f);
    cam.SetNearPlane(0.1f);
    cam.SetFarPlane(100.f);

    cam.SetVerticalFOV(glm::radians(45.f));
    const glm::mat4 p45 = cam.GetProjectionMatrix();

    cam.SetVerticalFOV(glm::radians(90.f));
    const glm::mat4 p90 = cam.GetProjectionMatrix();

    EXPECT_NE(p45[1][1], p90[1][1]);
}
