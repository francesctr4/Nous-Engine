#include <gtest/gtest.h>

#include <ResourceManager/Types/ResourceTexture/ResourceTexture.h>

// =====================================================
// Tests — ResourceTexture
// =====================================================

TEST(t_ResourceTexture, ConstructorSetsTypeTextureAndUID)
{
    ResourceTexture tex(99);
    EXPECT_EQ(tex.GetType(), ResourceType::TEXTURE);
    EXPECT_EQ(tex.GetUID(), 99u);
}

TEST(t_ResourceTexture, DefaultDimensionsAndChannelCountAreZero)
{
    ResourceTexture tex;
    EXPECT_EQ(tex.width,        0u);
    EXPECT_EQ(tex.height,       0u);
    EXPECT_EQ(tex.channelCount, 0u);
}

TEST(t_ResourceTexture, RefCountStartsAtZero)
{
    ResourceTexture tex(1);
    EXPECT_EQ(tex.GetReferenceCount(), 0u);
}
