#include <gtest/gtest.h>

#include <string>

#include <ResourceManager/Types/ResourceAudioGraph/ResourceAudioGraph.h>
#include <ResourceManager/Types/ResourceAudioGraph/ImporterAudioGraph.h>

// Scratch files go under the OS temp dir, not the working directory: CI runs
// ctest --parallel and two suites writing a same-named file into bin/ would race.
// Defined with the added tests at the bottom of this file.
namespace { std::string ScratchPath(const std::string& name); }

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

    const std::string path = ScratchPath("t_ResourceAudioGraph_roundtrip.nafx");
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

    const std::string path = ScratchPath("t_ResourceAudioGraph_partial.nafx");
    ASSERT_TRUE(ImporterAudioGraph::WriteAudioGraphToFile(src, path));

    ResourceAudioGraph loaded(7);
    ImporterAudioGraph importer;
    ASSERT_TRUE(importer.Deserialize(path, &loaded));

    ASSERT_EQ(loaded.effects.size(), 1u);
    ASSERT_EQ(loaded.effects[0].params.size(), 3u);  // schema-sized
    EXPECT_FLOAT_EQ(loaded.effects[0].params[2], 0.5f);  // wet → default
}

// ---------------------------------------------------------------------------
// Added coverage: the paths the two round-trip tests above do not reach --
// failure handling, schema evolution, chain ORDER, and the no-op GPU hooks.
//
// Chain order is the one to keep an eye on: AudioGraphDesc is a plain vector and
// "chain order == vector order" is the whole contract between the editor's
// linearizer and the runtime DSP splice. A reordering bug is inaudible in a test
// that only uses one effect.
// ---------------------------------------------------------------------------

#include <Utils/Serialization/JsonFile.h>
#include <Utils/Serialization/JsonObject.h>
#include <Utils/Serialization/JsonArray.h>

#include <cstdio>
#include <filesystem>

namespace
{
    namespace fs = std::filesystem;

    std::string ScratchPath(const std::string& name)
    {
        const fs::path dir = fs::temp_directory_path() / "nous_t_resourceaudiograph";
        fs::create_directories(dir);
        return (dir / name).string();
    }

    // Writes a hand-built .nafx so a test can describe JSON this build would
    // never itself emit (an unknown effect type, scrambled param order).
    std::string WriteRawNafx(const std::string& name, JsonObject&& root)
    {
        const std::string path = ScratchPath(name);
        JsonFile::SaveToFile(root, path);
        return path;
    }
}

TEST(t_ResourceAudioGraph, DeserializeMissingFileReturnsFalse)
{
    ResourceAudioGraph loaded(1);
    ImporterAudioGraph importer;

    EXPECT_FALSE(importer.Deserialize(ScratchPath("does_not_exist.nafx"), &loaded));
}

TEST(t_ResourceAudioGraph, DeserializeMalformedFileReturnsFalse)
{
    const std::string path = ScratchPath("malformed.nafx");
    {
        FILE* f = fopen(path.c_str(), "wb");
        ASSERT_NE(f, nullptr);
        fputs("{ not json at all", f);
        fclose(f);
    }

    ResourceAudioGraph loaded(1);
    ImporterAudioGraph importer;
    EXPECT_FALSE(importer.Deserialize(path, &loaded));
}

TEST(t_ResourceAudioGraph, EmptyGraphRoundTripsAsEmpty)
{
    const ResourceAudioGraph src(1);
    const std::string path = ScratchPath("empty.nafx");
    ASSERT_TRUE(ImporterAudioGraph::WriteAudioGraphToFile(src, path));

    ResourceAudioGraph loaded(1);
    ImporterAudioGraph importer;
    ASSERT_TRUE(importer.Deserialize(path, &loaded));

    EXPECT_TRUE(loaded.effects.empty());
    EXPECT_TRUE(loaded.editorPositions.empty());
}

TEST(t_ResourceAudioGraph, ChainOrderIsPreserved)
{
    // vector order IS signal order. Four distinct types, so a stable-but-wrong
    // ordering (sorted by enum value, say) cannot pass.
    ResourceAudioGraph src(2);
    src.effects = {
        { AudioEffectType::Gain,     { 0.9f } },
        { AudioEffectType::Delay,    { 0.1f, 0.2f, 0.3f } },
        { AudioEffectType::HighPass, { 300.0f } },
        { AudioEffectType::LowPass,  { 900.0f } },
    };

    const std::string path = ScratchPath("order.nafx");
    ASSERT_TRUE(ImporterAudioGraph::WriteAudioGraphToFile(src, path));

    ResourceAudioGraph loaded(2);
    ImporterAudioGraph importer;
    ASSERT_TRUE(importer.Deserialize(path, &loaded));

    ASSERT_EQ(loaded.effects.size(), 4u);
    EXPECT_EQ(loaded.effects[0].type, AudioEffectType::Gain);
    EXPECT_EQ(loaded.effects[1].type, AudioEffectType::Delay);
    EXPECT_EQ(loaded.effects[2].type, AudioEffectType::HighPass);
    EXPECT_EQ(loaded.effects[3].type, AudioEffectType::LowPass);
}

TEST(t_ResourceAudioGraph, RepeatedEffectTypeIsKeptAsSeparateEntries)
{
    // Two low-pass stages in series is a legitimate chain, not a duplicate to
    // be collapsed.
    ResourceAudioGraph src(3);
    src.effects = {
        { AudioEffectType::LowPass, { 400.0f } },
        { AudioEffectType::LowPass, { 900.0f } },
    };

    const std::string path = ScratchPath("repeat.nafx");
    ASSERT_TRUE(ImporterAudioGraph::WriteAudioGraphToFile(src, path));

    ResourceAudioGraph loaded(3);
    ImporterAudioGraph importer;
    ASSERT_TRUE(importer.Deserialize(path, &loaded));

    ASSERT_EQ(loaded.effects.size(), 2u);
    EXPECT_FLOAT_EQ(loaded.effects[0].params[0], 400.0f);
    EXPECT_FLOAT_EQ(loaded.effects[1].params[0], 900.0f);
}

TEST(t_ResourceAudioGraph, UnknownEffectTypeIsSkippedAndTheRestStillLoad)
{
    // Forward compatibility: a .nafx authored by a newer build that knows an
    // effect this one does not must still load its remaining effects rather than
    // failing the whole graph.
    JsonObject root;
    JsonArray effects;
    {
        JsonObject unknown;
        unknown.Set("type", "Reverb");          // not in this build's registry
        unknown.Set("params", JsonObject{});
        effects.Append(std::move(unknown));

        JsonObject gain;
        gain.Set("type", "Gain");
        JsonObject params;
        params.Set("gain", 0.75f);
        gain.Set("params", std::move(params));
        effects.Append(std::move(gain));
    }
    root.Set("effects", std::move(effects));

    const std::string path = WriteRawNafx("unknown_type.nafx", std::move(root));

    ResourceAudioGraph loaded(4);
    ImporterAudioGraph importer;
    ASSERT_TRUE(importer.Deserialize(path, &loaded));

    ASSERT_EQ(loaded.effects.size(), 1u);
    EXPECT_EQ(loaded.effects[0].type, AudioEffectType::Gain);
    EXPECT_FLOAT_EQ(loaded.effects[0].params[0], 0.75f);
}

TEST(t_ResourceAudioGraph, ParamsAreRepackedIntoSchemaOrderNotJsonOrder)
{
    // Params serialize BY NAME, so JSON key order is irrelevant; the loader must
    // re-pack them into the schema's order (delay, decay, wet). Written
    // deliberately back-to-front here.
    JsonObject root;
    JsonArray effects;
    {
        JsonObject delay;
        delay.Set("type", "Delay");
        JsonObject params;
        params.Set("wet",   0.11f);
        params.Set("decay", 0.22f);
        params.Set("delay", 0.33f);
        delay.Set("params", std::move(params));
        effects.Append(std::move(delay));
    }
    root.Set("effects", std::move(effects));

    const std::string path = WriteRawNafx("param_order.nafx", std::move(root));

    ResourceAudioGraph loaded(5);
    ImporterAudioGraph importer;
    ASSERT_TRUE(importer.Deserialize(path, &loaded));

    ASSERT_EQ(loaded.effects.size(), 1u);
    ASSERT_EQ(loaded.effects[0].params.size(), 3u);
    EXPECT_FLOAT_EQ(loaded.effects[0].params[0], 0.33f);   // delay
    EXPECT_FLOAT_EQ(loaded.effects[0].params[1], 0.22f);   // decay
    EXPECT_FLOAT_EQ(loaded.effects[0].params[2], 0.11f);   // wet
}

TEST(t_ResourceAudioGraph, EffectWithNoParamsBlockLoadsAllSchemaDefaults)
{
    JsonObject root;
    JsonArray effects;
    {
        JsonObject delay;
        delay.Set("type", "Delay");   // no "params" key at all
        effects.Append(std::move(delay));
    }
    root.Set("effects", std::move(effects));

    const std::string path = WriteRawNafx("no_params.nafx", std::move(root));

    ResourceAudioGraph loaded(6);
    ImporterAudioGraph importer;
    ASSERT_TRUE(importer.Deserialize(path, &loaded));

    ASSERT_EQ(loaded.effects.size(), 1u);
    ASSERT_EQ(loaded.effects[0].params.size(), 3u);
    EXPECT_FLOAT_EQ(loaded.effects[0].params[0], 0.25f);   // delay default
    EXPECT_FLOAT_EQ(loaded.effects[0].params[1], 0.40f);   // decay default
    EXPECT_FLOAT_EQ(loaded.effects[0].params[2], 0.50f);   // wet default
}

TEST(t_ResourceAudioGraph, DeserializeClearsPreviousContents)
{
    // The resource object is reused across a hot reload, so Deserialize must
    // replace rather than append -- otherwise every save doubles the chain.
    ResourceAudioGraph loaded(7);
    loaded.effects = { { AudioEffectType::Gain, { 0.5f } } };
    loaded.editorPositions = { glm::vec2(1.0f, 1.0f) };

    ResourceAudioGraph src(7);
    src.effects = { { AudioEffectType::LowPass, { 700.0f } } };
    src.editorPositions = { glm::vec2(9.0f, 9.0f) };

    const std::string path = ScratchPath("replace.nafx");
    ASSERT_TRUE(ImporterAudioGraph::WriteAudioGraphToFile(src, path));

    ImporterAudioGraph importer;
    ASSERT_TRUE(importer.Deserialize(path, &loaded));

    ASSERT_EQ(loaded.effects.size(), 1u);
    EXPECT_EQ(loaded.effects[0].type, AudioEffectType::LowPass);
    ASSERT_EQ(loaded.editorPositions.size(), 1u);
    EXPECT_FLOAT_EQ(loaded.editorPositions[0].x, 9.0f);
}

TEST(t_ResourceAudioGraph, EditorPositionsAreIndependentOfEffectCount)
{
    // The editor block carries two fixed anchor nodes (Source/Output) on top of
    // one node per effect, so the counts deliberately do not match. The runtime
    // ignores positions entirely; nothing may tie the two vectors together.
    ResourceAudioGraph src(8);
    src.effects = { { AudioEffectType::Gain, { 1.0f } } };
    src.editorPositions = { glm::vec2(0.f, 0.f), glm::vec2(1.f, 1.f), glm::vec2(2.f, 2.f) };

    const std::string path = ScratchPath("positions.nafx");
    ASSERT_TRUE(ImporterAudioGraph::WriteAudioGraphToFile(src, path));

    ResourceAudioGraph loaded(8);
    ImporterAudioGraph importer;
    ASSERT_TRUE(importer.Deserialize(path, &loaded));

    EXPECT_EQ(loaded.effects.size(), 1u);
    ASSERT_EQ(loaded.editorPositions.size(), 3u);
    EXPECT_FLOAT_EQ(loaded.editorPositions[2].y, 2.0f);
}

TEST(t_ResourceAudioGraph, CreateNewAudioGraphFileProducesALoadableEmptyGraph)
{
    const std::string path = ScratchPath("brand_new.nafx");
    ASSERT_TRUE(ImporterAudioGraph::CreateNewAudioGraphFile(path));
    ASSERT_TRUE(fs::exists(path));

    ResourceAudioGraph loaded(9);
    ImporterAudioGraph importer;
    ASSERT_TRUE(importer.Deserialize(path, &loaded));

    EXPECT_TRUE(loaded.effects.empty());
    EXPECT_TRUE(loaded.editorPositions.empty());
}

TEST(t_ResourceAudioGraph, CreateNewAudioGraphFileRejectsAnEmptyPath)
{
    EXPECT_FALSE(ImporterAudioGraph::CreateNewAudioGraphFile(""));
}

TEST(t_ResourceAudioGraph, GpuHooksAreNoOps)
{
    // AUDIO_GRAPH is a CPU-only resource type: the DSP chain is built at runtime
    // by the audio backend, not at resource load. Upload must still report
    // success, or ModuleRenderer3D's upload drain logs an error on every load.
    ResourceAudioGraph graph(10);
    ImporterAudioGraph importer;

    EXPECT_TRUE(importer.Upload(&graph, nullptr));
    EXPECT_NO_FATAL_FAILURE(importer.Release(&graph, nullptr));
    EXPECT_NO_FATAL_FAILURE(importer.Evict(&graph));
}

TEST(t_ResourceAudioGraph, DefaultResourceStateIsCpuOnly)
{
    const ResourceAudioGraph graph(11);

    EXPECT_EQ(graph.GetUID(), 11u);
    EXPECT_EQ(graph.generation, 0u);
    EXPECT_TRUE(graph.effects.empty());
    EXPECT_TRUE(graph.editorPositions.empty());
}
