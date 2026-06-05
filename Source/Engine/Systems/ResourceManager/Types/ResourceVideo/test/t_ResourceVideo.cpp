#include <gtest/gtest.h>

#include "Engine/Systems/ResourceManager/Types/ResourceVideo/include/ResourceVideo.h"

// ---------------- Descriptor ----------------

TEST(t_ResourceVideo, ConstructorSetsTypeVideoAndUID)
{
    ResourceVideo video(42);
    EXPECT_EQ(video.GetType(), ResourceType::VIDEO);
    EXPECT_EQ(video.GetUID(), 42u);
}

TEST(t_ResourceVideo, DefaultsAreSane)
{
    ResourceVideo video;
    EXPECT_EQ(video.GetFileType(),   VideoFileType::UNKNOWN);
    EXPECT_EQ(video.GetDecodeMode(), VideoDecodeMode::STREAMED);
    EXPECT_EQ(video.GetWidth(),      0u);
    EXPECT_EQ(video.GetHeight(),     0u);
    EXPECT_FLOAT_EQ(video.GetDurationSec(), 0.0f);
    EXPECT_FLOAT_EQ(video.GetFrameRate(),   0.0f);
    EXPECT_TRUE(video.GetCodecName().empty());
    EXPECT_FALSE(video.GetHasAudioTrack());
}

TEST(t_ResourceVideo, SettersRoundTrip)
{
    ResourceVideo video(1);
    video.SetFileType(VideoFileType::MP4);
    video.SetDecodeMode(VideoDecodeMode::PREDECODED);
    video.SetWidth(1920u);
    video.SetHeight(1080u);
    video.SetDurationSec(12.5f);
    video.SetFrameRate(30.0f);
    video.SetCodecName("h264");
    video.SetHasAudioTrack(true);

    EXPECT_EQ(video.GetFileType(),   VideoFileType::MP4);
    EXPECT_EQ(video.GetDecodeMode(), VideoDecodeMode::PREDECODED);
    EXPECT_EQ(video.GetWidth(),      1920u);
    EXPECT_EQ(video.GetHeight(),     1080u);
    EXPECT_FLOAT_EQ(video.GetDurationSec(), 12.5f);
    EXPECT_FLOAT_EQ(video.GetFrameRate(),   30.0f);
    EXPECT_EQ(video.GetCodecName(),  "h264");
    EXPECT_TRUE(video.GetHasAudioTrack());
}

// ---------------- Extension policy ----------------

TEST(t_ResourceVideo, FileTypeFromExtension)
{
    EXPECT_EQ(VideoFileTypeFromExtension("Library/Video/1.mp4"), VideoFileType::MP4);
    EXPECT_EQ(VideoFileTypeFromExtension("Library/Video/1.MP4"), VideoFileType::MP4);
    EXPECT_EQ(VideoFileTypeFromExtension("Library/Video/1.gif"), VideoFileType::GIF);
    EXPECT_EQ(VideoFileTypeFromExtension("Library/Video/1.GIF"), VideoFileType::GIF);
    EXPECT_EQ(VideoFileTypeFromExtension("Library/Video/1.mov"), VideoFileType::UNKNOWN);
}

TEST(t_ResourceVideo, DecodeModeFromFileType)
{
    EXPECT_EQ(VideoDecodeModeFromFileType(VideoFileType::MP4),     VideoDecodeMode::STREAMED);
    EXPECT_EQ(VideoDecodeModeFromFileType(VideoFileType::GIF),     VideoDecodeMode::PREDECODED);
    EXPECT_EQ(VideoDecodeModeFromFileType(VideoFileType::UNKNOWN), VideoDecodeMode::STREAMED);
}
