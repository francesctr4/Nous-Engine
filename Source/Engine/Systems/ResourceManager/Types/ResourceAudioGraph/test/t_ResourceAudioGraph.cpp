#include <gtest/gtest.h>

#include "Engine/Systems/ResourceManager/Types/ResourceAudioGraph/include/ResourceAudioGraph.h"
#include "Engine/Systems/ResourceManager/Types/ResourceAudioGraph/include/ImporterAudioGraph.h"

// Round-trips a graph through .nafx JSON: write → Deserialize → compare.
// Engine-linked (JsonFile + the importer live in the engine DLL).

TEST(t_ResourceAudioGraph, EffectsAndParamsRoundTrip)
{
    ResourceAudioGraph src(123);
    src.effects = {
        { AudioEffectType::LowPass, { 800.0f } },
        { AudioEffectType::Delay,   { 0.3f, 0.5f, 0.4f } }
    };
    src.editorPositions = { glm::vec2(40.0f, 40.0f), glm::vec2(260.0f, 40.0f) };

    const std::string path = "t_ResourceAudioGraph_roundtrip.nafx";
    ASSERT_TRUE(ImporterAudioGraph::WriteAudioGraphToFile(src, path));

    ResourceAudioGraph loaded(123);
    ImporterAudioGraph importer;  // m_resources unused — no external refs
    ASSERT_TRUE(importer.Deserialize(path, &loaded));

    ASSERT_EQ(loaded.effects.size(), 2u);
    EXPECT_EQ(loaded.effects[0].type, AudioEffectType::LowPass);
    ASSERT_EQ(loaded.effects[0].params.size(), 1u);
    EXPECT_FLOAT_EQ(loaded.effects[0].params[0], 800.0f);

    EXPECT_EQ(loaded.effects[1].type, AudioEffectType::Delay);
    ASSERT_EQ(loaded.effects[1].params.size(), 3u);
    EXPECT_FLOAT_EQ(loaded.effects[1].params[0], 0.3f);
    EXPECT_FLOAT_EQ(loaded.effects[1].params[1], 0.5f);
    EXPECT_FLOAT_EQ(loaded.effects[1].params[2], 0.4f);

    ASSERT_EQ(loaded.editorPositions.size(), 2u);
    EXPECT_FLOAT_EQ(loaded.editorPositions[1].x, 260.0f);
}

TEST(t_ResourceAudioGraph, MissingParamFallsBackToSchemaDefault)
{
    // A .nafx where a Delay omits "wet" should load wet at its schema default (0.5).
    ResourceAudioGraph src(7);
    src.effects = { { AudioEffectType::Delay, { 0.3f, 0.5f } } };  // only delay + decay written

    const std::string path = "t_ResourceAudioGraph_partial.nafx";
    ASSERT_TRUE(ImporterAudioGraph::WriteAudioGraphToFile(src, path));

    ResourceAudioGraph loaded(7);
    ImporterAudioGraph importer;
    ASSERT_TRUE(importer.Deserialize(path, &loaded));

    ASSERT_EQ(loaded.effects.size(), 1u);
    ASSERT_EQ(loaded.effects[0].params.size(), 3u);  // schema-sized
    EXPECT_FLOAT_EQ(loaded.effects[0].params[2], 0.5f);  // wet → default
}
