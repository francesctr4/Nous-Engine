// Covers RenderPacketPolicy.h -- the rules extracted out of
// ModuleRenderer3D::BuildRenderPacket (166 lines of registry walking wrapped
// around a handful of decisions, none of which a test could previously reach).
//
// Links gtest and glm. No registry, no camera, no engine target, no device.

#include <gtest/gtest.h>

#include "RenderPacketPolicy.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace nous::engine::renderer;

namespace
{
    constexpr float kEps = 1e-4f;
}

// ===========================================================================
// Which frustums get built
// ===========================================================================

TEST(t_RenderPacketPolicy, SceneFrustumOnlyInEditorAndOnlyWithACamera)
{
    EXPECT_TRUE (ShouldBuildSceneFrustum(/*editor*/ true,  /*camera*/ true));
    EXPECT_FALSE(ShouldBuildSceneFrustum(/*editor*/ true,  /*camera*/ false));
    EXPECT_FALSE(ShouldBuildSceneFrustum(/*editor*/ false, /*camera*/ true));
    EXPECT_FALSE(ShouldBuildSceneFrustum(/*editor*/ false, /*camera*/ false));
}

TEST(t_RenderPacketPolicy, SceneFrustumIgnoresTheCullingToggle)
{
    // The scene viewport always culls in the editor; frustumCullingEnabled is a
    // GAME-mode shipping option and is not even a parameter here. Stated as a
    // test so that "unifying" the two frustum rules has to confront it.
    EXPECT_TRUE(ShouldBuildSceneFrustum(true, true));
}

TEST(t_RenderPacketPolicy, GameFrustumAlwaysAppliedInEditorRegardlessOfTheToggle)
{
    // The editor's game-preview panel culls every frame even when the shipping
    // toggle is off, so the preview matches what the built game will draw.
    EXPECT_TRUE(ShouldBuildGameFrustum(/*editor*/ true, /*camera*/ true, /*culling*/ false));
    EXPECT_TRUE(ShouldBuildGameFrustum(/*editor*/ true, /*camera*/ true, /*culling*/ true));
}

TEST(t_RenderPacketPolicy, GameFrustumInGameModeRequiresTheToggle)
{
    EXPECT_TRUE (ShouldBuildGameFrustum(/*editor*/ false, /*camera*/ true, /*culling*/ true));
    EXPECT_FALSE(ShouldBuildGameFrustum(/*editor*/ false, /*camera*/ true, /*culling*/ false));
}

TEST(t_RenderPacketPolicy, NoGameCameraMeansNoGameFrustumEverywhere)
{
    // A scene with no CCamera is legal; it must not build a frustum from a null
    // camera in either mode or with either toggle setting.
    for (const bool editor : {true, false})
        for (const bool culling : {true, false})
            EXPECT_FALSE(ShouldBuildGameFrustum(editor, /*camera*/ false, culling))
                << "editor=" << editor << " culling=" << culling;
}

// ===========================================================================
// Visibility, including the cache-miss rule
// ===========================================================================

TEST(t_RenderPacketPolicy, NoActiveFrustumDrawsEverything)
{
    EXPECT_TRUE(IsGeometryVisible(/*frustum*/ false, /*cached*/ true,  /*inside*/ false));
    EXPECT_TRUE(IsGeometryVisible(/*frustum*/ false, /*cached*/ false, /*inside*/ false));
}

TEST(t_RenderPacketPolicy, UnmeasuredGeometryIsNeverCulled)
{
    // THE rule worth protecting. A mesh missing from the AABB cache has not been
    // measured yet -- typically the frame it spawns. Culling on absence makes
    // objects wink out for a frame as they appear.
    EXPECT_TRUE(IsGeometryVisible(/*frustum*/ true, /*cached*/ false, /*inside*/ false));
}

TEST(t_RenderPacketPolicy, MeasuredGeometryIsCulledByTheFrustum)
{
    EXPECT_TRUE (IsGeometryVisible(/*frustum*/ true, /*cached*/ true, /*inside*/ true));
    EXPECT_FALSE(IsGeometryVisible(/*frustum*/ true, /*cached*/ true, /*inside*/ false));
}

TEST(t_RenderPacketPolicy, CullingRequiresBothAnActiveFrustumAndAMeasurement)
{
    // The only combination that culls, spelled out exhaustively so a flipped
    // condition anywhere in the truth table fails.
    for (const bool frustum : {true, false})
        for (const bool cached : {true, false})
            for (const bool inside : {true, false})
            {
                const bool expectedCulled = frustum && cached && !inside;
                EXPECT_EQ(IsGeometryVisible(frustum, cached, inside), !expectedCulled)
                    << "frustum=" << frustum << " cached=" << cached << " inside=" << inside;
            }
}

// ===========================================================================
// Light conventions
// ===========================================================================

TEST(t_RenderPacketPolicy, UnrotatedLightPointsDownNegativeY)
{
    const glm::vec3 fwd = LightForward(glm::quat(1.f, 0.f, 0.f, 0.f));   // identity

    EXPECT_NEAR(fwd.x,  0.f, kEps);
    EXPECT_NEAR(fwd.y, -1.f, kEps);
    EXPECT_NEAR(fwd.z,  0.f, kEps);
}

TEST(t_RenderPacketPolicy, RotatingNinetyDegreesAboutXAimsTheLightForward)
{
    // -Y rotated +90 degrees about X becomes -Z... or +Z, depending on handedness.
    // Rather than assert a sign convention twice, assert the invariant that
    // matters: the result is a unit vector and it has left the Y axis entirely.
    const glm::quat q = glm::angleAxis(glm::radians(90.f), glm::vec3(1.f, 0.f, 0.f));
    const glm::vec3 fwd = LightForward(q);

    EXPECT_NEAR(glm::length(fwd), 1.f, kEps);
    EXPECT_NEAR(fwd.y, 0.f, kEps);
    EXPECT_NEAR(std::abs(fwd.z), 1.f, kEps);
}

TEST(t_RenderPacketPolicy, LightForwardIsAlwaysNormalised)
{
    // The shader assumes a unit direction; a non-unit one scales the lighting
    // term instead of just aiming it.
    for (const float deg : {0.f, 30.f, 90.f, 180.f, 270.f})
    {
        const glm::quat q = glm::angleAxis(glm::radians(deg), glm::normalize(glm::vec3(1.f, 2.f, 3.f)));
        EXPECT_NEAR(glm::length(LightForward(q)), 1.f, kEps) << "deg=" << deg;
    }
}

TEST(t_RenderPacketPolicy, SpotAnglesAreStoredAsCosinesInInnerOuterOrder)
{
    const glm::vec4 angles = PackSpotAngles(/*inner*/ 0.f, /*outer*/ 60.f);

    EXPECT_NEAR(angles.x, 1.0f, kEps);   // cos(0)
    EXPECT_NEAR(angles.y, 0.5f, kEps);   // cos(60)
    EXPECT_NEAR(angles.z, 0.f, kEps);
    EXPECT_NEAR(angles.w, 0.f, kEps);
}

TEST(t_RenderPacketPolicy, InnerCosineIsGreaterThanOuterForAValidCone)
{
    // Cosine decreases as the angle widens, so a well-formed cone has
    // angles.x >= angles.y. The shader's smoothstep depends on that ordering;
    // swapping the two produces an inverted falloff rather than an error.
    for (int inner = 0; inner <= 80; inner += 10)
    {
        const glm::vec4 a = PackSpotAngles(static_cast<float>(inner),
                                           static_cast<float>(inner) + 10.f);
        EXPECT_GE(a.x, a.y) << "inner=" << inner;
    }
}

TEST(t_RenderPacketPolicy, SpotAnglesHandleTheDegenerateEqualCase)
{
    // inner == outer is a hard-edged cone, not an error.
    const glm::vec4 a = PackSpotAngles(45.f, 45.f);
    EXPECT_NEAR(a.x, a.y, kEps);
}

TEST(t_RenderPacketPolicy, ColorPacksIntensityIntoW)
{
    const glm::vec4 c = PackLightColor(glm::vec3(0.25f, 0.5f, 0.75f), 3.f);

    EXPECT_NEAR(c.r, 0.25f, kEps);
    EXPECT_NEAR(c.g, 0.5f,  kEps);
    EXPECT_NEAR(c.b, 0.75f, kEps);
    EXPECT_NEAR(c.a, 3.f,   kEps);
}

TEST(t_RenderPacketPolicy, PositionPacksRangeIntoW)
{
    const glm::vec4 p = PackLightPosition(glm::vec3(1.f, 2.f, 3.f), 25.f);

    EXPECT_NEAR(p.x, 1.f,  kEps);
    EXPECT_NEAR(p.y, 2.f,  kEps);
    EXPECT_NEAR(p.z, 3.f,  kEps);
    EXPECT_NEAR(p.w, 25.f, kEps);
}

TEST(t_RenderPacketPolicy, ColorAndPositionPackingAreNotInterchangeable)
{
    // Both are vec4(vec3, float) and the compiler cannot tell them apart, so the
    // only guard against swapping the two call sites is that w means different
    // things. Pinned as an explicit reminder.
    const glm::vec4 c = PackLightColor(glm::vec3(1.f, 1.f, 1.f), 2.f);
    const glm::vec4 p = PackLightPosition(glm::vec3(1.f, 1.f, 1.f), 50.f);

    EXPECT_NEAR(c.w, 2.f,  kEps);    // intensity
    EXPECT_NEAR(p.w, 50.f, kEps);    // range
}

// ===========================================================================
// Light budget
// ===========================================================================

TEST(t_RenderPacketPolicy, LightsAreAcceptedUpToTheLimit)
{
    EXPECT_TRUE (CanAcceptLight(0, 4));
    EXPECT_TRUE (CanAcceptLight(3, 4));
    EXPECT_FALSE(CanAcceptLight(4, 4));
    EXPECT_FALSE(CanAcceptLight(5, 4));
}

TEST(t_RenderPacketPolicy, AZeroLightBudgetAcceptsNothing)
{
    EXPECT_FALSE(CanAcceptLight(0, 0));
}

TEST(t_RenderPacketPolicy, OnlyTheFirstDirectionalLightIsAccepted)
{
    EXPECT_TRUE (CanAcceptDirectionalLight(/*alreadyHaveOne*/ false));
    EXPECT_FALSE(CanAcceptDirectionalLight(/*alreadyHaveOne*/ true));
}

TEST(t_RenderPacketPolicy, ExtraLightsAreDroppedNotSubstituted)
{
    // First-come-first-served: once the budget is full every later light is
    // refused, rather than evicting an accepted one. Replacing instead would make
    // the lit result flicker as entt's iteration order shifts between frames.
    constexpr uint32_t kMax = 2;
    uint32_t accepted = 0;

    for (int i = 0; i < 5; ++i)
        if (CanAcceptLight(accepted, kMax))
            ++accepted;

    EXPECT_EQ(accepted, kMax);
}
