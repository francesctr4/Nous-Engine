#include <gtest/gtest.h>

#include <AnimationSystem/AnimParameters.h>

using nous::engine::animation_system::AnimParameters;

// =============================================================================
// Round-trip per type
// =============================================================================

TEST(t_AnimParameters, SetAndGetFloat)
{
    AnimParameters p;
    p.SetFloat("speed", 3.5f);
    EXPECT_FLOAT_EQ(p.GetFloat("speed"), 3.5f);
}

TEST(t_AnimParameters, SetAndGetBool)
{
    AnimParameters p;
    p.SetBool("isGrounded", true);
    EXPECT_TRUE(p.GetBool("isGrounded"));

    p.SetBool("isGrounded", false);
    EXPECT_FALSE(p.GetBool("isGrounded"));
}

// =============================================================================
// Unknown names
// =============================================================================

TEST(t_AnimParameters, GetOnAnUnknownNameReturnsTheFallback)
{
    const AnimParameters p;
    EXPECT_FLOAT_EQ(p.GetFloat("nope"), 0.0f);
    EXPECT_FLOAT_EQ(p.GetFloat("nope", 7.0f), 7.0f);
    EXPECT_FALSE(p.GetBool("nope"));
    EXPECT_TRUE(p.GetBool("nope", true));
}

// A script polling GetFloat in Update runs this thousands of times. If a read
// created the entry, the store would grow every frame forever -- a leak that
// presents as a memory bug rather than an API one.
TEST(t_AnimParameters, GetDoesNotCreateAnEntry)
{
    AnimParameters p;
    for (int i = 0; i < 100; ++i)
        (void)p.GetFloat("neverSet");

    EXPECT_EQ(p.Count(), 0u);
}

TEST(t_AnimParameters, ReSettingANameUpdatesRatherThanDuplicating)
{
    AnimParameters p;
    p.SetFloat("speed", 1.0f);
    p.SetFloat("speed", 2.0f);

    EXPECT_EQ(p.Count(), 1u);
    EXPECT_FLOAT_EQ(p.GetFloat("speed"), 2.0f);
}

// =============================================================================
// Type safety
// =============================================================================

// Reading a bool as a float is a script bug. Returning 1.0 would hide it; the
// fallback makes it visible without needing an assert in a dependency-free lib.
TEST(t_AnimParameters, ACrossTypeReadReturnsTheFallback)
{
    AnimParameters p;
    p.SetBool("isGrounded", true);
    EXPECT_FLOAT_EQ(p.GetFloat("isGrounded", -1.0f), -1.0f);

    p.SetFloat("speed", 3.0f);
    EXPECT_TRUE(p.GetBool("speed", true));     // fallback, not 3.0 -> true
    EXPECT_FALSE(p.GetBool("speed", false));
}

// =============================================================================
// Triggers
// =============================================================================

TEST(t_AnimParameters, ATriggerPersistsUntilConsumed)
{
    AnimParameters p;
    p.SetTrigger("jump");
    EXPECT_TRUE(p.IsTriggerSet("jump"));

    // Still set after an arbitrary number of frames -- Unity's semantics. Nothing
    // consumes triggers until the controller graph exists.
    EXPECT_TRUE(p.IsTriggerSet("jump"));

    EXPECT_TRUE(p.ConsumeTrigger("jump"));
    EXPECT_FALSE(p.IsTriggerSet("jump"));
    EXPECT_FALSE(p.ConsumeTrigger("jump"));
}

TEST(t_AnimParameters, ResetTriggerClearsWithoutConsuming)
{
    AnimParameters p;
    p.SetTrigger("jump");
    p.ResetTrigger("jump");

    EXPECT_FALSE(p.IsTriggerSet("jump"));
    EXPECT_FALSE(p.ConsumeTrigger("jump"));
}

TEST(t_AnimParameters, ResetTriggerOnAnUnsetNameIsANoOp)
{
    AnimParameters p;
    p.ResetTrigger("neverSet");

    EXPECT_FALSE(p.IsTriggerSet("neverSet"));
    EXPECT_EQ(p.Count(), 0u);
}

TEST(t_AnimParameters, ConsumeTriggerOnAnUnknownNameReturnsFalse)
{
    AnimParameters p;
    EXPECT_FALSE(p.ConsumeTrigger("neverSet"));
    EXPECT_EQ(p.Count(), 0u);
}

// =============================================================================
// Independence
// =============================================================================

TEST(t_AnimParameters, ParametersAreIndependent)
{
    AnimParameters p;
    p.SetFloat("speed", 4.0f);
    p.SetBool("isGrounded", true);
    p.SetTrigger("jump");

    EXPECT_EQ(p.Count(), 3u);
    EXPECT_FLOAT_EQ(p.GetFloat("speed"), 4.0f);
    EXPECT_TRUE(p.GetBool("isGrounded"));
    EXPECT_TRUE(p.IsTriggerSet("jump"));

    EXPECT_TRUE(p.ConsumeTrigger("jump"));
    EXPECT_FLOAT_EQ(p.GetFloat("speed"), 4.0f);   // untouched by the consume
    EXPECT_TRUE(p.GetBool("isGrounded"));
}

// =============================================================================
// Presence
// =============================================================================

// The distinction no getter can express: every one of them folds a missing name into
// the caller's fallback, so "absent" and "present and legitimately zero" look
// identical from outside. Seeding a controller's declared defaults turns on exactly
// this -- a default fills an empty slot and must never overwrite a script's value.
TEST(t_AnimParameters, ContainsDistinguishesAbsenceFromAZeroValue)
{
    AnimParameters p;

    EXPECT_FALSE(p.Contains("speed"));

    p.SetFloat("speed", 0.0f);
    EXPECT_TRUE(p.Contains("speed"));     // present, and legitimately zero

    p.SetBool("grounded", false);
    EXPECT_TRUE(p.Contains("grounded"));

    p.SetTrigger("jump");
    EXPECT_TRUE(p.ConsumeTrigger("jump"));
    EXPECT_TRUE(p.Contains("jump"));      // consumed, not removed

    // A Get never creates an entry -- a script polling one every frame must not grow
    // the store, and Contains must not start reporting a name nothing ever set.
    (void)p.GetFloat("neverSet", 1.0f);
    EXPECT_FALSE(p.Contains("neverSet"));
}
