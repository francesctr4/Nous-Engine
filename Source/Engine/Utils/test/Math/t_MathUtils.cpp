#include <gtest/gtest.h>

#include <Utils/Math/MathUtils.h>
#include <Utils/Math/FrustumCulling.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

// =============================================================================
// MathUtils constants
// =============================================================================

TEST(t_MathUtils, PI_HasCorrectValue)
{
    EXPECT_NEAR(NOUS_MathUtils::PI, 3.14159265f, 1e-6f);
}

TEST(t_MathUtils, TAU_Is2TimesPI)
{
    EXPECT_NEAR(NOUS_MathUtils::TAU, 2.0f * NOUS_MathUtils::PI, 1e-5f);
}

TEST(t_MathUtils, HALF_PI_IsHalfOfPI)
{
    EXPECT_NEAR(NOUS_MathUtils::HALF_PI, NOUS_MathUtils::PI / 2.0f, 1e-6f);
}

TEST(t_MathUtils, DEGTORAD_RADTODEG_AreInverses)
{
    EXPECT_NEAR(NOUS_MathUtils::DEGTORAD * NOUS_MathUtils::RADTODEG, 1.0f, 1e-5f);
}

TEST(t_MathUtils, DEGTORAD_90Degrees_IsHalfPI)
{
    EXPECT_NEAR(90.0f * NOUS_MathUtils::DEGTORAD, NOUS_MathUtils::HALF_PI, 1e-5f);
}

TEST(t_MathUtils, SQRT_2_IsCorrect)
{
    EXPECT_NEAR(NOUS_MathUtils::SQRT_2 * NOUS_MathUtils::SQRT_2, 2.0f, 1e-5f);
}

// =============================================================================
// MathUtils templates
// =============================================================================

TEST(t_MathUtils, MIN_Int_ReturnsSmaller)
{
    EXPECT_EQ(NOUS_MathUtils::MIN(3, 5), 3);
    EXPECT_EQ(NOUS_MathUtils::MIN(5, 3), 3);
}

TEST(t_MathUtils, MIN_Float_ReturnsSmaller)
{
    EXPECT_FLOAT_EQ(NOUS_MathUtils::MIN(1.5f, 2.5f), 1.5f);
}

TEST(t_MathUtils, MIN_EqualValues_ReturnsValue)
{
    EXPECT_EQ(NOUS_MathUtils::MIN(7, 7), 7);
}

TEST(t_MathUtils, MAX_Int_ReturnsLarger)
{
    EXPECT_EQ(NOUS_MathUtils::MAX(3, 5), 5);
    EXPECT_EQ(NOUS_MathUtils::MAX(5, 3), 5);
}

TEST(t_MathUtils, MAX_Float_ReturnsLarger)
{
    EXPECT_FLOAT_EQ(NOUS_MathUtils::MAX(1.5f, 2.5f), 2.5f);
}

TEST(t_MathUtils, MAX_EqualValues_ReturnsValue)
{
    EXPECT_EQ(NOUS_MathUtils::MAX(7, 7), 7);
}

// =============================================================================
// FrustumCulling
// =============================================================================

namespace
{
    // Camera at origin, looking down -Z. 90-degree vertical FOV, 1:1 aspect.
    FrustumCulling::FrustumPlanes MakeTestFrustum()
    {
        const glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 1.0f, 100.0f);
        const glm::mat4 view = glm::lookAt(
            glm::vec3(0.0f, 0.0f,  0.0f),   // eye
            glm::vec3(0.0f, 0.0f, -1.0f),   // center
            glm::vec3(0.0f, 1.0f,  0.0f));  // up
        return FrustumCulling::ExtractFrustumPlanes(proj * view);
    }
}

TEST(t_FrustumCulling, BoxCenteredInFrustum_IsVisible)
{
    const auto frustum = MakeTestFrustum();
    // A 2×2×2 box at z=-10, well inside the frustum
    EXPECT_TRUE(FrustumCulling::IsAABBVisible(frustum,
        glm::vec3(-1.0f, -1.0f, -11.0f),
        glm::vec3( 1.0f,  1.0f,  -9.0f)));
}

TEST(t_FrustumCulling, BoxBehindCamera_IsNotVisible)
{
    const auto frustum = MakeTestFrustum();
    // Box entirely behind the near plane (positive Z from a -Z-looking camera)
    EXPECT_FALSE(FrustumCulling::IsAABBVisible(frustum,
        glm::vec3(-1.0f, -1.0f, 2.0f),
        glm::vec3( 1.0f,  1.0f, 5.0f)));
}

TEST(t_FrustumCulling, BoxBeyondFarPlane_IsNotVisible)
{
    const auto frustum = MakeTestFrustum();
    // Box at z=-200, past the far plane of 100
    EXPECT_FALSE(FrustumCulling::IsAABBVisible(frustum,
        glm::vec3(-1.0f, -1.0f, -210.0f),
        glm::vec3( 1.0f,  1.0f, -190.0f)));
}

TEST(t_FrustumCulling, BoxFarToTheSide_IsNotVisible)
{
    const auto frustum = MakeTestFrustum();
    // A box far to the right (X=200), out of the 90-degree FOV
    EXPECT_FALSE(FrustumCulling::IsAABBVisible(frustum,
        glm::vec3(199.0f, -1.0f, -10.0f),
        glm::vec3(201.0f,  1.0f,  -9.0f)));
}
