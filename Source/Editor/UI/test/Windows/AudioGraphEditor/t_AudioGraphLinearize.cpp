#include <gtest/gtest.h>

#include <EditorUI/AudioGraphLinearize.h>

// Pure graph→ordered-desc logic. No imgui-node-editor runtime — operates on plain
// node/link descriptors.

using namespace nous::audio_editor;

TEST(t_AudioGraphLinearize, OrdersEffectsSourceToOutput)
{
    // Source(1) → LowPass(2) → Delay(3) → Output(4)
    std::vector<LinNode> nodes = {
        { 1, LinNodeKind::Source, AudioEffectType::LowPass /*ignored*/, {} },
        { 2, LinNodeKind::Effect, AudioEffectType::LowPass, { 800.0f } },
        { 3, LinNodeKind::Effect, AudioEffectType::Delay,   { 0.3f, 0.5f, 0.4f } },
        { 4, LinNodeKind::Output, AudioEffectType::LowPass /*ignored*/, {} },
    };
    std::vector<LinLink> links = { {1, 2}, {2, 3}, {3, 4} };  // {fromNodeId, toNodeId}

    AudioGraphDesc desc;
    ASSERT_TRUE(Linearize(nodes, links, desc));
    ASSERT_EQ(desc.size(), 2u);
    EXPECT_EQ(desc[0].type, AudioEffectType::LowPass);
    ASSERT_EQ(desc[0].params.size(), 1u);
    EXPECT_FLOAT_EQ(desc[0].params[0], 800.0f);
    EXPECT_EQ(desc[1].type, AudioEffectType::Delay);
}

TEST(t_AudioGraphLinearize, EmptyChainSourceDirectToOutput)
{
    std::vector<LinNode> nodes = {
        { 1, LinNodeKind::Source, AudioEffectType::Gain, {} },
        { 2, LinNodeKind::Output, AudioEffectType::Gain, {} },
    };
    std::vector<LinLink> links = { {1, 2} };
    AudioGraphDesc desc;
    ASSERT_TRUE(Linearize(nodes, links, desc));
    EXPECT_TRUE(desc.empty());
}

TEST(t_AudioGraphLinearize, DisconnectedReturnsFalse)
{
    std::vector<LinNode> nodes = {
        { 1, LinNodeKind::Source, AudioEffectType::Gain, {} },
        { 2, LinNodeKind::Effect, AudioEffectType::Gain, { 1.0f } },
        { 3, LinNodeKind::Output, AudioEffectType::Gain, {} },
    };
    std::vector<LinLink> links = { {1, 2} };  // 2 → 3 missing
    AudioGraphDesc desc;
    EXPECT_FALSE(Linearize(nodes, links, desc));
}
