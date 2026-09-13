#include <AnimationSystem/Transform.h>

#include <gtest/gtest.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

using namespace nous::engine::animation_system;

namespace
{
    constexpr float kEps = 1e-5f;

    void ExpectVec3Near(const glm::vec3& a, const glm::vec3& b, float eps = kEps)
    {
        EXPECT_NEAR(a.x, b.x, eps);
        EXPECT_NEAR(a.y, b.y, eps);
        EXPECT_NEAR(a.z, b.z, eps);
    }

    // Quaternions q and -q are the same rotation, so comparing components directly
    // fails on a legitimate sign flip. Compare the rotations instead.
    void ExpectRotationNear(const glm::quat& a, const glm::quat& b, float eps = kEps)
    {
        EXPECT_NEAR(std::abs(glm::dot(a, b)), 1.0f, eps);
    }
}

TEST(t_Interpolate, IdentityTransformToMatrixIsIdentity)
{
    EXPECT_EQ(Transform{}.ToMatrix(), glm::mat4(1.0f));
}

TEST(t_Interpolate, ToMatrixIsTranslateRotateScale)
{
    Transform t;
    t.position = { 1.0f, 2.0f, 3.0f };
    t.rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    t.scale    = { 2.0f, 2.0f, 2.0f };

    const glm::mat4 expected =
        glm::translate(glm::mat4(1.0f), t.position) *
        glm::mat4_cast(t.rotation) *
        glm::scale(glm::mat4(1.0f), t.scale);

    const glm::mat4 actual = t.ToMatrix();

    for (int c = 0; c < 4; ++c)
    {
        for (int r = 0; r < 4; ++r) EXPECT_NEAR(actual[c][r], expected[c][r], kEps);
    }
}

TEST(t_Interpolate, WeightZeroReturnsAExactly)
{
    Transform a;
    a.position = { 1.0f, 2.0f, 3.0f };
    a.rotation = glm::angleAxis(glm::radians(30.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    a.scale    = { 0.5f, 0.5f, 0.5f };

    Transform b;
    b.position = { -4.0f, 8.0f, 0.25f };
    b.rotation = glm::angleAxis(glm::radians(120.0f), glm::vec3(1.0f, 0.0f, 0.0f));

    const Transform result = Interpolate(a, b, 0.0f);

    // Bit-exact, not "near": a cross-fade that has not started must hand back the
    // source pose untouched.
    EXPECT_EQ(result.position, a.position);
    EXPECT_EQ(result.scale,    a.scale);
    EXPECT_EQ(result.rotation, a.rotation);
}

TEST(t_Interpolate, WeightOneReturnsBExactly)
{
    Transform a;
    a.position = { 1.0f, 2.0f, 3.0f };

    Transform b;
    b.position = { -4.0f, 8.0f, 0.25f };
    b.rotation = glm::angleAxis(glm::radians(120.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    b.scale    = { 3.0f, 1.0f, 1.0f };

    const Transform result = Interpolate(a, b, 1.0f);

    EXPECT_EQ(result.position, b.position);
    EXPECT_EQ(result.scale,    b.scale);
    EXPECT_EQ(result.rotation, b.rotation);
}

TEST(t_Interpolate, WeightIsClampedOutsideUnitRange)
{
    Transform a; a.position = { 0.0f, 0.0f, 0.0f };
    Transform b; b.position = { 10.0f, 0.0f, 0.0f };

    EXPECT_EQ(Interpolate(a, b, -5.0f).position, a.position);
    EXPECT_EQ(Interpolate(a, b,  5.0f).position, b.position);
}

TEST(t_Interpolate, MidpointLerpsPositionAndScale)
{
    Transform a;
    a.position = { 0.0f, 0.0f, 0.0f };
    a.scale    = { 1.0f, 1.0f, 1.0f };

    Transform b;
    b.position = { 10.0f, -4.0f, 2.0f };
    b.scale    = { 3.0f, 3.0f, 3.0f };

    const Transform mid = Interpolate(a, b, 0.5f);

    ExpectVec3Near(mid.position, { 5.0f, -2.0f, 1.0f });
    ExpectVec3Near(mid.scale,    { 2.0f, 2.0f, 2.0f });
}

TEST(t_Interpolate, MidpointSlerpsRotationToHalfAngle)
{
    const glm::vec3 axis(0.0f, 1.0f, 0.0f);

    Transform a; a.rotation = glm::angleAxis(glm::radians(0.0f),  axis);
    Transform b; b.rotation = glm::angleAxis(glm::radians(90.0f), axis);

    const Transform mid = Interpolate(a, b, 0.5f);

    ExpectRotationNear(mid.rotation, glm::angleAxis(glm::radians(45.0f), axis));
}

// The test the spec singles out. Without short-arc handling a 181-degree rotation
// interpolates the 179 degrees the other way, and the visible symptom is an arm
// sweeping through the torso rather than around it.
TEST(t_Interpolate, SlerpTakesShortArc)
{
    const glm::vec3 axis(0.0f, 0.0f, 1.0f);

    Transform a; a.rotation = glm::angleAxis(glm::radians(0.0f),   axis);
    Transform b; b.rotation = glm::angleAxis(glm::radians(350.0f), axis);

    const Transform mid = Interpolate(a, b, 0.5f);

    // Short arc: 0 -> 350 is -10 degrees, so the midpoint is -5 (== 355), NOT 175.
    ExpectRotationNear(mid.rotation, glm::angleAxis(glm::radians(355.0f), axis));

    // State it the other way too, so a regression cannot pass by accident: the
    // interpolated rotation must stay within the short arc of both endpoints.
    const glm::quat longWay = glm::angleAxis(glm::radians(175.0f), axis);
    EXPECT_LT(std::abs(glm::dot(mid.rotation, longWay)), 0.5f);
}

TEST(t_Interpolate, SlerpResultStaysNormalized)
{
    Transform a; a.rotation = glm::angleAxis(glm::radians(17.0f), glm::normalize(glm::vec3(1, 2, 3)));
    Transform b; b.rotation = glm::angleAxis(glm::radians(203.0f), glm::normalize(glm::vec3(-3, 1, 0.5f)));

    for (float t : { 0.1f, 0.25f, 0.5f, 0.75f, 0.9f })
    {
        EXPECT_NEAR(glm::length(Interpolate(a, b, t).rotation), 1.0f, kEps) << "t = " << t;
    }
}
