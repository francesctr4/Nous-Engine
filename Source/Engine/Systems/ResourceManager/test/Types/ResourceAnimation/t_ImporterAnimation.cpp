#include <gtest/gtest.h>

#include <ResourceManager/Core/MetaFileData.h>
#include <ResourceManager/Types/ResourceAnimation/ImporterAnimation.h>
#include <ResourceManager/Types/ResourceAnimation/ResourceAnimation.h>

#include <filesystem>
#include <fstream>
#include <string>

using nous::engine::animation_system::AnimChannel;
using nous::engine::animation_system::AnimClipData;

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

    ASSERT_TRUE(ImporterAnimation::SaveClip(meta, source));

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
    ASSERT_TRUE(ImporterAnimation::SaveClip(meta, Clip()));

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
    ASSERT_TRUE(ImporterAnimation::SaveClip(meta, Clip()));

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
    ASSERT_TRUE(ImporterAnimation::SaveClip(meta, Clip()));

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
    ASSERT_TRUE(ImporterAnimation::SaveClip(meta, Clip()));

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

    ASSERT_TRUE(ImporterAnimation::SaveClip(meta, empty));

    ResourceAnimation loaded(meta.uid);
    ImporterAnimation importer;
    ASSERT_TRUE(importer.Deserialize(meta.libraryPath, &loaded));

    EXPECT_EQ(loaded.clip.name, "Take 001");
    EXPECT_EQ(loaded.clip.ChannelCount(), 0u);

    std::filesystem::remove(meta.libraryPath);
}

TEST(t_ImporterAnimation, HasNoGpuResidency)
{
    ResourceAnimation resource(1);
    resource.clip = Clip();

    ImporterAnimation importer;
    EXPECT_TRUE(importer.Upload(&resource, nullptr));
    importer.Release(&resource, nullptr);

    importer.Evict(&resource);
    EXPECT_EQ(resource.clip.ChannelCount(), 0u);
}
