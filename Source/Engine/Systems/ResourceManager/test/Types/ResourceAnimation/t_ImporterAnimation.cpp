#include <gtest/gtest.h>

#include <ResourceManager/Core/MetaFileData.h>
#include <ResourceManager/Types/ResourceAnimation/ImporterAnimation.h>
#include <ResourceManager/Types/ResourceAnimation/ResourceAnimation.h>
#include <Utils/Serialization/JsonArray.h>
#include <Utils/Serialization/JsonFile.h>
#include <Utils/Serialization/JsonObject.h>

#include <filesystem>
#include <fstream>
#include <string>

using nous::engine::animation_system::AnimChannel;
using nous::engine::animation_system::AnimClipData;
using nous::engine::animation_system::AnimationEvent;

namespace
{
    // Scratch files go under the OS temp dir, not the working directory: CI runs
    // ctest --parallel and two suites writing a same-named file into bin/ would race.
    std::string ScratchPath(const std::string& name)
    {
        return (std::filesystem::temp_directory_path() / name).string();
    }

    AnimClipData Clip()
    {
        AnimClipData clip;
        clip.name     = "mixamo.com";
        clip.duration = 2.3667f;

        // Every component of every key distinct, so a transposed read cannot pass by
        // coincidence -- and asymmetric quaternions, so a w/x swap is visible.
        AnimChannel hips;
        hips.boneName    = "mixamorig:Hips";
        hips.posTimes    = { 0.0f, 1.0f, 2.0f };
        hips.posValues   = { {1,2,3}, {4,5,6}, {7,8,9} };
        hips.rotTimes    = { 0.0f, 2.0f };
        hips.rotValues   = { glm::quat(0.1f, 0.2f, 0.3f, 0.4f),
                             glm::quat(0.5f, 0.6f, 0.7f, 0.8f) };
        hips.scaleTimes  = { 0.0f };
        hips.scaleValues = { {1.5f, 2.5f, 3.5f} };

        // The three tracks are independent, and an exporter routinely writes only
        // one of them.
        AnimChannel prop;
        prop.boneName  = "Prop";
        prop.posTimes  = { 0.5f };
        prop.posValues = { {3, 4, 5} };

        clip.channels = { hips, prop };
        return clip;
    }

    MetaFileData MetaFor(const std::string& fileName)
    {
        MetaFileData meta;
        meta.uid          = 777;
        meta.name         = "TestClip";
        meta.resourceType = ResourceType::ANIMATION;
        meta.assetsPath   = "Assets/TestClip.nanim";
        meta.libraryPath  = ScratchPath(fileName);
        return meta;
    }
}

TEST(t_ImporterAnimation, RoundTripsEveryTrackThroughTheRealWriter)
{
    const MetaFileData meta   = MetaFor("t_ImporterAnimation_roundtrip.nanim");
    const AnimClipData source = Clip();

    ASSERT_TRUE(ImporterAnimation::SaveClip(meta, source, {}));

    ResourceAnimation loaded(meta.uid);
    ImporterAnimation importer;
    ASSERT_TRUE(importer.Deserialize(meta.libraryPath, &loaded));

    EXPECT_EQ(loaded.clip.name, "mixamo.com");
    ASSERT_EQ(loaded.clip.ChannelCount(), 2u);

    const AnimChannel& hips = loaded.clip.channels[0];
    EXPECT_EQ(hips.boneName,    "mixamorig:Hips");
    EXPECT_EQ(hips.posTimes,    source.channels[0].posTimes);
    EXPECT_EQ(hips.posValues,   source.channels[0].posValues);
    EXPECT_EQ(hips.rotTimes,    source.channels[0].rotTimes);
    EXPECT_EQ(hips.rotValues,   source.channels[0].rotValues);
    EXPECT_EQ(hips.scaleTimes,  source.channels[0].scaleTimes);
    EXPECT_EQ(hips.scaleValues, source.channels[0].scaleValues);

    std::filesystem::remove(meta.libraryPath);
}

// glm::quat's memory layout is {x,y,z,w} -- w is the LAST member -- while this
// format stores w,x,y,z. Dumping the value vector's raw bytes would therefore write
// the components in the wrong order, and a symmetric test quaternion would not
// notice. Every component here is distinct.
TEST(t_ImporterAnimation, RotationComponentsSurviveInTheRightOrder)
{
    const MetaFileData meta = MetaFor("t_ImporterAnimation_quat.nanim");
    ASSERT_TRUE(ImporterAnimation::SaveClip(meta, Clip(), {}));

    ResourceAnimation loaded(meta.uid);
    ImporterAnimation importer;
    ASSERT_TRUE(importer.Deserialize(meta.libraryPath, &loaded));

    const glm::quat& q = loaded.clip.channels[0].rotValues[0];
    EXPECT_FLOAT_EQ(q.w, 0.1f);
    EXPECT_FLOAT_EQ(q.x, 0.2f);
    EXPECT_FLOAT_EQ(q.y, 0.3f);
    EXPECT_FLOAT_EQ(q.z, 0.4f);

    std::filesystem::remove(meta.libraryPath);
}

TEST(t_ImporterAnimation, RoundTripsAChannelWithPositionKeysOnly)
{
    const MetaFileData meta = MetaFor("t_ImporterAnimation_partial.nanim");
    ASSERT_TRUE(ImporterAnimation::SaveClip(meta, Clip(), {}));

    ResourceAnimation loaded(meta.uid);
    ImporterAnimation importer;
    ASSERT_TRUE(importer.Deserialize(meta.libraryPath, &loaded));

    ASSERT_EQ(loaded.clip.ChannelCount(), 2u);
    const AnimChannel& prop = loaded.clip.channels[1];

    EXPECT_EQ(prop.boneName, "Prop");
    ASSERT_EQ(prop.posTimes.size(), 1u);
    EXPECT_FLOAT_EQ(prop.posTimes[0], 0.5f);
    EXPECT_EQ(prop.posValues[0], glm::vec3(3, 4, 5));

    EXPECT_TRUE(prop.rotTimes.empty());
    EXPECT_TRUE(prop.rotValues.empty());
    EXPECT_TRUE(prop.scaleTimes.empty());
    EXPECT_TRUE(prop.scaleValues.empty());

    std::filesystem::remove(meta.libraryPath);
}

// Duration is SECONDS by the time it reaches here -- ticks died at BuildClip. A
// lossy round-trip would shift the loop point, which reads as an animation bug
// rather than a format bug.
TEST(t_ImporterAnimation, PreservesDurationExactly)
{
    const MetaFileData meta = MetaFor("t_ImporterAnimation_duration.nanim");
    ASSERT_TRUE(ImporterAnimation::SaveClip(meta, Clip(), {}));

    ResourceAnimation loaded(meta.uid);
    ImporterAnimation importer;
    ASSERT_TRUE(importer.Deserialize(meta.libraryPath, &loaded));

    EXPECT_FLOAT_EQ(loaded.clip.duration, 2.3667f);

    std::filesystem::remove(meta.libraryPath);
}

TEST(t_ImporterAnimation, RejectsAForeignMagicRatherThanParsingIt)
{
    const MetaFileData meta = MetaFor("t_ImporterAnimation_badmagic.nanim");
    {
        std::ofstream out(meta.libraryPath, std::ios::binary);
        const uint32_t wrongMagic = 0x4E534B4Cu;   // the SKELETON magic
        out.write(reinterpret_cast<const char*>(&wrongMagic), sizeof(wrongMagic));
    }

    ResourceAnimation loaded(meta.uid);
    ImporterAnimation importer;
    EXPECT_FALSE(importer.Deserialize(meta.libraryPath, &loaded));
    EXPECT_EQ(loaded.clip.ChannelCount(), 0u);

    std::filesystem::remove(meta.libraryPath);
}

TEST(t_ImporterAnimation, RejectsATruncatedFile)
{
    const MetaFileData meta = MetaFor("t_ImporterAnimation_truncated.nanim");
    ASSERT_TRUE(ImporterAnimation::SaveClip(meta, Clip(), {}));

    const auto full = std::filesystem::file_size(meta.libraryPath);
    std::filesystem::resize_file(meta.libraryPath, full / 2);

    ResourceAnimation loaded(meta.uid);
    ImporterAnimation importer;
    EXPECT_FALSE(importer.Deserialize(meta.libraryPath, &loaded));

    std::filesystem::remove(meta.libraryPath);
}

// A channel-less clip never reaches the writer in practice -- PlanModelAssets drops
// those -- but the format must not be the thing that breaks if one does.
TEST(t_ImporterAnimation, WritesAClipWithNoChannels)
{
    const MetaFileData meta = MetaFor("t_ImporterAnimation_empty.nanim");

    AnimClipData empty;
    empty.name     = "Take 001";
    empty.duration = 3.3333f;

    ASSERT_TRUE(ImporterAnimation::SaveClip(meta, empty, {}));

    ResourceAnimation loaded(meta.uid);
    ImporterAnimation importer;
    ASSERT_TRUE(importer.Deserialize(meta.libraryPath, &loaded));

    EXPECT_EQ(loaded.clip.name, "Take 001");
    EXPECT_EQ(loaded.clip.ChannelCount(), 0u);

    std::filesystem::remove(meta.libraryPath);
}

// =============================================================================
// Per-clip settings
// =============================================================================

TEST(t_ImporterAnimation, ANewClipLoopsAtNormalSpeed)
{
    const ResourceAnimation fresh(1);
    EXPECT_TRUE(fresh.settings.loop);
    EXPECT_FLOAT_EQ(fresh.settings.speed, 1.0f);
}

TEST(t_ImporterAnimation, RoundTripsPerClipSettings)
{
    const MetaFileData meta = MetaFor("t_ImporterAnimation_settings.nanim");

    ClipAuthoring authored;
    authored.settings.loop  = false;
    authored.settings.speed = 2.5f;

    ASSERT_TRUE(ImporterAnimation::SaveClip(meta, Clip(), authored));

    ResourceAnimation loaded(meta.uid);
    ImporterAnimation importer;
    ASSERT_TRUE(importer.Deserialize(meta.libraryPath, &loaded));

    EXPECT_FALSE(loaded.settings.loop);
    EXPECT_FLOAT_EQ(loaded.settings.speed, 2.5f);

    // The settings sit between `duration` and `channelCount`, so a writer and reader
    // that disagreed on their size would shift every channel that follows. Reading
    // the first channel back is what makes that desync visible here rather than as a
    // truncated-file error in an unrelated test.
    ASSERT_EQ(loaded.clip.ChannelCount(), 2u);
    EXPECT_EQ(loaded.clip.channels[0].boneName, "mixamorig:Hips");

    std::filesystem::remove(meta.libraryPath);
}

// EnsureStub writes only "source" and "clip", so a stub that has never been edited
// declares no settings at all. That is the shipping path, not a legacy one.
TEST(t_ImporterAnimation, ReadsDefaultSettingsFromAStubThatDeclaresNone)
{
    const std::string stub = ScratchPath("t_ImporterAnimation_plain.nanim");
    {
        JsonObject json;
        json.Set("source", "Assets/Rig.fbx");
        json.Set("clip",   "mixamo.com");
        ASSERT_TRUE(JsonFile::SaveToFile(json, stub));
    }

    const ClipAuthoring authoring = ImporterAnimation::ReadAuthoringFromStub(stub);
    EXPECT_TRUE(authoring.settings.loop);
    EXPECT_FLOAT_EQ(authoring.settings.speed, 1.0f);
    EXPECT_TRUE(authoring.events.empty());

    std::filesystem::remove(stub);
}

// The stub is the copy that survives a Library/ nuke, so the write-back must leave
// the two fields the fallback re-parse depends on alone.
TEST(t_ImporterAnimation, WritingSettingsToAStubKeepsItsSourceAndClipKeys)
{
    const std::string stub = ScratchPath("t_ImporterAnimation_stub.nanim");
    {
        JsonObject json;
        json.Set("source", "Assets/Rig.fbx");
        json.Set("clip",   "mixamo.com");
        ASSERT_TRUE(JsonFile::SaveToFile(json, stub));
    }

    ClipAuthoring edited;
    edited.settings.loop  = false;
    edited.settings.speed = 0.25f;
    ASSERT_TRUE(ImporterAnimation::WriteAuthoringToStub(stub, edited));

    const JsonObject reloaded = JsonFile::LoadFromFile(stub);
    EXPECT_EQ(reloaded.GetString("source"), "Assets/Rig.fbx");
    EXPECT_EQ(reloaded.GetString("clip"),   "mixamo.com");

    const ClipAuthoring readBack = ImporterAnimation::ReadAuthoringFromStub(stub);
    EXPECT_FALSE(readBack.settings.loop);
    EXPECT_FLOAT_EQ(readBack.settings.speed, 0.25f);

    std::filesystem::remove(stub);
}

// The editor's entry point, and the reason it is one call: the two copies exist for
// different readers (the stub survives a Library/ nuke, the binary is what a shipped
// game reads), so writing one and not the other leaves them disagreeing until the
// next re-import silently picks a winner.
TEST(t_ImporterAnimation, SaveAuthoringUpdatesBothTheStubAndTheBinary)
{
    const std::string stub    = ScratchPath("t_ImporterAnimation_both.nanim");
    const std::string library = ScratchPath("t_ImporterAnimation_both_lib.nanim");
    {
        JsonObject json;
        json.Set("source", "Assets/Rig.fbx");
        json.Set("clip",   "mixamo.com");
        ASSERT_TRUE(JsonFile::SaveToFile(json, stub));
    }

    ResourceAnimation animation(42);
    animation.SetAssetsPath(stub);
    animation.SetLibraryPath(library);
    animation.clip           = Clip();
    animation.settings.loop  = false;
    animation.settings.speed = 1.75f;

    ASSERT_TRUE(ImporterAnimation::SaveAuthoring(animation));

    const ClipAuthoring fromStub = ImporterAnimation::ReadAuthoringFromStub(stub);
    EXPECT_FALSE(fromStub.settings.loop);
    EXPECT_FLOAT_EQ(fromStub.settings.speed, 1.75f);

    ResourceAnimation loaded(42);
    ImporterAnimation importer;
    ASSERT_TRUE(importer.Deserialize(library, &loaded));
    EXPECT_FALSE(loaded.settings.loop);
    EXPECT_FLOAT_EQ(loaded.settings.speed, 1.75f);

    std::filesystem::remove(stub);
    std::filesystem::remove(library);
}

TEST(t_ImporterAnimation, SaveWritesTheResourcesOwnSettings)
{
    const MetaFileData meta = MetaFor("t_ImporterAnimation_save.nanim");

    ResourceAnimation source(meta.uid);
    source.clip           = Clip();
    source.settings.loop  = false;
    source.settings.speed = 3.0f;

    ImporterAnimation importer;
    ResourceBase* asBase = &source;
    ASSERT_TRUE(importer.Save(meta, asBase));

    ResourceAnimation loaded(meta.uid);
    ASSERT_TRUE(importer.Deserialize(meta.libraryPath, &loaded));

    EXPECT_FALSE(loaded.settings.loop);
    EXPECT_FLOAT_EQ(loaded.settings.speed, 3.0f);

    std::filesystem::remove(meta.libraryPath);
}

// =============================================================================
// Animation events
// =============================================================================

// Events ride in the binary because that is the copy an exported game ships. The
// assertion on the first channel's bone name AFTER them is the point: a writer and
// reader that disagreed on the events block's size would shift every channel that
// follows, and only a post-events check catches that.
TEST(t_ImporterAnimation, EventsRoundTripThroughTheRealWriter)
{
    const MetaFileData meta = MetaFor("t_ImporterAnimation_events.nanim");
    const AnimClipData source = Clip();

    ClipAuthoring authoring;
    authoring.settings.loop  = false;
    authoring.settings.speed = 1.5f;
    authoring.events.push_back({ 0.25f, "Footstep", 0.0f, "L" });
    authoring.events.push_back({ 0.75f, "Footstep", 1.0f, "R" });
    authoring.events.push_back({ 1.00f, "Hit",      3.5f, "" });

    ASSERT_TRUE(ImporterAnimation::SaveClip(meta, source, authoring));

    ResourceAnimation loaded(meta.uid);
    ImporterAnimation importer;
    ASSERT_TRUE(importer.Deserialize(meta.libraryPath, &loaded));

    ASSERT_EQ(loaded.events.size(), 3u);
    EXPECT_FLOAT_EQ(loaded.events[0].time, 0.25f);
    EXPECT_EQ(loaded.events[0].name, "Footstep");
    EXPECT_EQ(loaded.events[0].stringParam, "L");
    EXPECT_FLOAT_EQ(loaded.events[2].floatParam, 3.5f);
    EXPECT_EQ(loaded.events[2].name, "Hit");
    EXPECT_TRUE(loaded.events[2].stringParam.empty());

    EXPECT_FALSE(loaded.settings.loop);
    EXPECT_FLOAT_EQ(loaded.settings.speed, 1.5f);

    // Everything AFTER the events block must still line up.
    ASSERT_EQ(loaded.clip.ChannelCount(), source.channels.size());
    EXPECT_EQ(loaded.clip.channels[0].boneName, source.channels[0].boneName);

    std::filesystem::remove(meta.libraryPath);
}

TEST(t_ImporterAnimation, AClipWithNoEventsRoundTripsEmpty)
{
    const MetaFileData meta = MetaFor("t_ImporterAnimation_noevents.nanim");
    ASSERT_TRUE(ImporterAnimation::SaveClip(meta, Clip(), ClipAuthoring{}));

    ResourceAnimation loaded(meta.uid);
    ImporterAnimation importer;
    ASSERT_TRUE(importer.Deserialize(meta.libraryPath, &loaded));

    EXPECT_TRUE(loaded.events.empty());

    std::filesystem::remove(meta.libraryPath);
}

// The writer sorts, so the runtime's emission order is time order no matter how the
// stub or the timeline window happened to append them.
TEST(t_ImporterAnimation, TheWriterSortsEventsByTime)
{
    const MetaFileData meta = MetaFor("t_ImporterAnimation_sortevents.nanim");

    ClipAuthoring authoring;
    authoring.events.push_back({ 0.9f, "late",  0.0f, "" });
    authoring.events.push_back({ 0.1f, "early", 0.0f, "" });

    ASSERT_TRUE(ImporterAnimation::SaveClip(meta, Clip(), authoring));

    ResourceAnimation loaded(meta.uid);
    ImporterAnimation importer;
    ASSERT_TRUE(importer.Deserialize(meta.libraryPath, &loaded));

    ASSERT_EQ(loaded.events.size(), 2u);
    EXPECT_EQ(loaded.events[0].name, "early");

    std::filesystem::remove(meta.libraryPath);
}

// The stub is the authoring copy and the ONLY thing that survives a Library/ nuke.
// Read-modify-write, so the keys the fallback re-parse needs are preserved.
TEST(t_ImporterAnimation, TheStubRoundTripsEventsAndKeepsSourceAndClip)
{
    const std::string stub = ScratchPath("t_ImporterAnimation_stubevents.nanim");
    {
        JsonObject json;
        json.Set("source", "Assets/Walk.fbx");
        json.Set("clip",   "mixamo.com");
        ASSERT_TRUE(JsonFile::SaveToFile(json, stub));
    }

    ClipAuthoring authoring;
    authoring.settings.speed = 2.0f;
    authoring.events.push_back({ 0.4f, "Footstep", 0.0f, "L" });
    ASSERT_TRUE(ImporterAnimation::WriteAuthoringToStub(stub, authoring));

    const ClipAuthoring read = ImporterAnimation::ReadAuthoringFromStub(stub);
    ASSERT_EQ(read.events.size(), 1u);
    EXPECT_EQ(read.events[0].name, "Footstep");
    EXPECT_EQ(read.events[0].stringParam, "L");
    EXPECT_FLOAT_EQ(read.events[0].time, 0.4f);
    EXPECT_FLOAT_EQ(read.settings.speed, 2.0f);

    const JsonObject after = JsonFile::LoadFromFile(stub);
    EXPECT_EQ(after.GetString("source"), "Assets/Walk.fbx");
    EXPECT_EQ(after.GetString("clip"),   "mixamo.com");

    std::filesystem::remove(stub);
}

// An unnamed event can match nothing a script tests for, so it is authoring noise
// rather than data -- dropping it keeps the runtime list meaningful.
TEST(t_ImporterAnimation, AnUnnamedStubEventIsDropped)
{
    // The stub must exist first: WriteAuthoringToStub is read-modify-write, so that
    // the "source"/"clip" keys the fallback re-parse needs survive. EnsureStub is
    // what creates it in the real pipeline.
    const std::string stub = ScratchPath("t_ImporterAnimation_unnamed.nanim");
    {
        JsonObject json;
        json.Set("source", "Assets/Rig.fbx");
        json.Set("clip",   "mixamo.com");
        ASSERT_TRUE(JsonFile::SaveToFile(json, stub));
    }

    ClipAuthoring authoring;
    authoring.events.push_back({ 0.4f, "",      0.0f, "" });
    authoring.events.push_back({ 0.6f, "Named", 0.0f, "" });
    ASSERT_TRUE(ImporterAnimation::WriteAuthoringToStub(stub, authoring));

    const ClipAuthoring read = ImporterAnimation::ReadAuthoringFromStub(stub);
    ASSERT_EQ(read.events.size(), 1u);
    EXPECT_EQ(read.events[0].name, "Named");

    std::filesystem::remove(stub);
}

TEST(t_ImporterAnimation, HasNoGpuResidency)
{
    ResourceAnimation resource(1);
    resource.clip = Clip();
    resource.events.push_back({ 0.5f, "Hit", 0.0f, "" });

    ImporterAnimation importer;
    EXPECT_TRUE(importer.Upload(&resource, nullptr));
    importer.Release(&resource, nullptr);

    importer.Evict(&resource);
    EXPECT_EQ(resource.clip.ChannelCount(), 0u);
    EXPECT_TRUE(resource.events.empty());
}
