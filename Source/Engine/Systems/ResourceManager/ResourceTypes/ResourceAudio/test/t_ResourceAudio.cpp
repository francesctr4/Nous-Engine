#include <gtest/gtest.h>

#include "Engine/Systems/ResourceManager/ResourceTypes/ResourceAudio/include/ResourceAudio.h"

// =====================================================
// Tests — ResourceAudio
// =====================================================

TEST(t_ResourceAudio, ConstructorSetsTypeAudioAndUID)
{
    ResourceAudio audio(42);
    EXPECT_EQ(audio.GetType(), ResourceType::AUDIO);
    EXPECT_EQ(audio.GetUID(), 42u);
}

TEST(t_ResourceAudio, DefaultsAreSane)
{
    ResourceAudio audio;
    EXPECT_EQ(audio.GetStreamingMode(), AudioStreamingMode::DECODED);
    EXPECT_EQ(audio.GetFileType(),      AudioFileType::UNKNOWN);
    EXPECT_FLOAT_EQ(audio.GetDurationSec(), 0.0f);
    EXPECT_EQ(audio.GetSampleRate(),    0u);
    EXPECT_EQ(audio.GetChannelCount(),  0u);
    EXPECT_EQ(audio.GetInternalData(),  nullptr);
}

TEST(t_ResourceAudio, RefCountStartsAtZero)
{
    ResourceAudio audio(1);
    EXPECT_EQ(audio.GetReferenceCount(), 0u);
}

TEST(t_ResourceAudio, SettersRoundTrip)
{
    ResourceAudio audio(1);
    audio.SetStreamingMode(AudioStreamingMode::STREAMED);
    audio.SetFileType(AudioFileType::OGG);
    audio.SetDurationSec(12.5f);
    audio.SetSampleRate(48000u);
    audio.SetChannelCount(2u);

    EXPECT_EQ(audio.GetStreamingMode(), AudioStreamingMode::STREAMED);
    EXPECT_EQ(audio.GetFileType(),      AudioFileType::OGG);
    EXPECT_FLOAT_EQ(audio.GetDurationSec(), 12.5f);
    EXPECT_EQ(audio.GetSampleRate(),    48000u);
    EXPECT_EQ(audio.GetChannelCount(),  2u);
}

TEST(t_ResourceAudio, InternalDataPointerRoundTrip)
{
    ResourceAudio audio(1);
    int sentinel = 0;
    audio.SetInternalData(&sentinel);
    EXPECT_EQ(audio.GetInternalData(), &sentinel);

    audio.SetInternalData(nullptr);
    EXPECT_EQ(audio.GetInternalData(), nullptr);
}
