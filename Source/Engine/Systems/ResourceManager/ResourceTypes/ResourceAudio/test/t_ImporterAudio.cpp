#include <gtest/gtest.h>

#include "Engine/Systems/ResourceManager/ResourceTypes/ResourceAudio/include/ImporterAudio.h"
#include "Engine/Systems/ResourceManager/ResourceTypes/ResourceAudio/include/ResourceAudio.h"
#include "Engine/Systems/ResourceManager/Core/Resource/MetaFileData.inl"
#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/Core/Globals.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// =============================================================================
// Helpers — generate a minimal PCM WAV file on disk
// =============================================================================

namespace
{
    const std::string kTempDir =
        (fs::temp_directory_path() / "nous_t_ImporterAudio").string();

    // Writes a PCM WAV file with sane defaults: 1 second long.
    // Header layout is the standard 44-byte RIFF/WAVE/fmt /data tree.
    void WritePcmWav(const std::string& path,
                     uint16_t channels,
                     uint32_t sampleRate,
                     uint16_t bitsPerSample,
                     float    durationSec)
    {
        const uint32_t frameCount  = static_cast<uint32_t>(sampleRate * durationSec);
        const uint16_t bytesPerSample = bitsPerSample / 8;
        const uint16_t blockAlign  = channels * bytesPerSample;
        const uint32_t byteRate    = sampleRate * blockAlign;
        const uint32_t dataSize    = frameCount * blockAlign;
        const uint32_t riffSize    = 36 + dataSize;

        std::ofstream f(path, std::ios::binary);
        auto W = [&f](const void* p, std::streamsize n) {
            f.write(reinterpret_cast<const char*>(p), n);
        };
        auto W4 = [&](const char* s) { f.write(s, 4); };

        const uint32_t fmtChunkSize = 16;
        const uint16_t audioFormat  = 1; // PCM

        W4("RIFF"); W(&riffSize, 4); W4("WAVE");
        W4("fmt "); W(&fmtChunkSize, 4);
        W(&audioFormat, 2); W(&channels, 2);
        W(&sampleRate, 4);  W(&byteRate, 4);
        W(&blockAlign, 2);  W(&bitsPerSample, 2);
        W4("data"); W(&dataSize, 4);

        // Silent samples — zeros are fine for probe testing.
        std::vector<uint8_t> samples(dataSize, 0);
        if (!samples.empty()) W(samples.data(), static_cast<std::streamsize>(samples.size()));
    }
}

// =============================================================================
// Fixture
// =============================================================================

class t_ImporterAudio : public ::testing::Test
{
protected:
    void SetUp() override
    {
        nous::engine::memory::InitializeMemory(MiB(8));
        fs::create_directories(kTempDir);

        m_assetsPath  = (fs::path(kTempDir) / "source.wav").string();
        m_libraryPath = (fs::path(kTempDir) / "12345.wav").string();
    }

    void TearDown() override
    {
        std::error_code ec;
        fs::remove_all(kTempDir, ec);
        nous::engine::memory::ShutdownMemory();
    }

    MetaFileData MakeMeta() const
    {
        MetaFileData meta;
        meta.name         = "TestAudio";
        meta.uid          = 12345;
        meta.resourceType = ResourceType::AUDIO;
        meta.assetsPath   = m_assetsPath;
        meta.libraryPath  = m_libraryPath;
        return meta;
    }

    std::string m_assetsPath;
    std::string m_libraryPath;
};

// =============================================================================
// Save — copies source to library path
// =============================================================================

TEST_F(t_ImporterAudio, Save_CopiesSourceToLibraryPath)
{
    WritePcmWav(m_assetsPath, /*ch*/1, /*sr*/8000, /*bps*/16, /*sec*/1.0f);

    MetaFileData meta = MakeMeta();
    Resource* res = NOUS_NEW<ResourceAudio>(MemoryTag::RESOURCE_AUDIO);
    ImporterAudio importer;

    EXPECT_TRUE(importer.Save(meta, res));
    EXPECT_TRUE(fs::exists(m_libraryPath));
    // Save deletes res internally — do not free it here.
}

TEST_F(t_ImporterAudio, Save_MissingSource_ReturnsFalse)
{
    // Don't write a source file — Save's CopyFile should fail.
    MetaFileData meta = MakeMeta();
    Resource* res = NOUS_NEW<ResourceAudio>(MemoryTag::RESOURCE_AUDIO);
    ImporterAudio importer;

    EXPECT_FALSE(importer.Save(meta, res));
}

// =============================================================================
// Deserialize — probe roundtrip
// =============================================================================

TEST_F(t_ImporterAudio, Deserialize_PopulatesProbeMetadata_Wav)
{
    constexpr uint16_t kChannels   = 2;
    constexpr uint32_t kSampleRate = 16000;
    constexpr float    kDuration   = 0.5f;
    WritePcmWav(m_libraryPath, kChannels, kSampleRate, 16, kDuration);

    ResourceAudio audio(12345);
    ImporterAudio importer;
    ASSERT_TRUE(importer.Deserialize(m_libraryPath, &audio));

    EXPECT_EQ(audio.GetFileType(),    AudioFileType::WAV);
    EXPECT_EQ(audio.GetSampleRate(),  kSampleRate);
    EXPECT_EQ(audio.GetChannelCount(), kChannels);
    EXPECT_NEAR(audio.GetDurationSec(), kDuration, 0.01f);
}

TEST_F(t_ImporterAudio, Deserialize_MissingFile_ReturnsFalse)
{
    ResourceAudio audio(12345);
    ImporterAudio importer;
    EXPECT_FALSE(importer.Deserialize("nonexistent/path/audio.wav", &audio));
}

TEST_F(t_ImporterAudio, Deserialize_UnknownExtension_SetsFileTypeUnknown)
{
    // File body is a valid WAV, but path uses an unrecognized extension.
    const std::string oddPath = (fs::path(kTempDir) / "weird.xyz").string();
    WritePcmWav(oddPath, 1, 8000, 16, 0.1f);

    ResourceAudio audio(1);
    ImporterAudio importer;
    // miniaudio sniffs the body, not the extension, so it still decodes.
    ASSERT_TRUE(importer.Deserialize(oddPath, &audio));
    EXPECT_EQ(audio.GetFileType(), AudioFileType::UNKNOWN);
}

// =============================================================================
// Save → Deserialize roundtrip via meta paths
// =============================================================================

TEST_F(t_ImporterAudio, SaveThenDeserialize_ProducesProbedResource)
{
    constexpr uint16_t kChannels   = 1;
    constexpr uint32_t kSampleRate = 22050;
    constexpr float    kDuration   = 0.25f;
    WritePcmWav(m_assetsPath, kChannels, kSampleRate, 16, kDuration);

    MetaFileData meta = MakeMeta();
    Resource* res = NOUS_NEW<ResourceAudio>(MemoryTag::RESOURCE_AUDIO);
    ImporterAudio importer;
    ASSERT_TRUE(importer.Save(meta, res));

    ResourceAudio audio(meta.uid);
    ASSERT_TRUE(importer.Deserialize(meta.libraryPath, &audio));
    EXPECT_EQ(audio.GetFileType(),     AudioFileType::WAV);
    EXPECT_EQ(audio.GetSampleRate(),   kSampleRate);
    EXPECT_EQ(audio.GetChannelCount(), kChannels);
    EXPECT_NEAR(audio.GetDurationSec(), kDuration, 0.01f);
}

// =============================================================================
// Evict / Upload / Release — MVP no-ops; verify they don't crash or fail.
// =============================================================================

TEST_F(t_ImporterAudio, Evict_IsNoOp)
{
    ResourceAudio audio(1);
    ImporterAudio importer;
    EXPECT_NO_THROW(importer.Evict(&audio));
}

TEST_F(t_ImporterAudio, Upload_ReturnsTrue)
{
    ResourceAudio audio(1);
    ImporterAudio importer;
    EXPECT_TRUE(importer.Upload(&audio, nullptr));
}

TEST_F(t_ImporterAudio, Release_IsNoOp)
{
    ResourceAudio audio(1);
    ImporterAudio importer;
    EXPECT_NO_THROW(importer.Release(&audio, nullptr));
}
