#include <gtest/gtest.h>

#include "Engine/Systems/ResourceManager/Importer/ImporterTexture/include/ImporterTexture.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceTexture/include/ResourceTexture.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/Core/Globals.h"

#include <filesystem>
#include <fstream>
#include <cstdint>
#include <string>

namespace fs = std::filesystem;

// =============================================================================
// BMP write helper — produces a minimal 1×1 24-bit BMP readable by stb_image
// =============================================================================

static void WriteMinimalBmp(const std::string& path, uint8_t r, uint8_t g, uint8_t b)
{
    // File header (14 bytes) + BITMAPINFOHEADER (40 bytes) + pixel row (4 bytes) = 58 bytes
    const uint8_t bmp[58] = {
        // File header
        'B', 'M',
        58,  0,   0,   0,   // file size
        0,   0,   0,   0,   // reserved
        54,  0,   0,   0,   // pixel data offset
        // BITMAPINFOHEADER
        40,  0,   0,   0,   // header size
        1,   0,   0,   0,   // width  = 1
        1,   0,   0,   0,   // height = 1
        1,   0,             // color planes
        24,  0,             // bits per pixel
        0,   0,   0,   0,   // compression = BI_RGB
        4,   0,   0,   0,   // image size (1 pixel + 1 pad byte)
        19,  11,  0,   0,   // X pixels/m
        19,  11,  0,   0,   // Y pixels/m
        0,   0,   0,   0,   // colors in table
        0,   0,   0,   0,   // important colors
        // Pixel row: BGR + padding (BMP stores bottom-up, BGR order)
        b, g, r, 0
    };
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(bmp), sizeof(bmp));
}

// Writes a 2×1 BMP: left pixel opaque white, right pixel transparent black.
// (24-bit BMP has no alpha — stb_image fills the forced alpha=255, so
//  hasTransparency can only be tested with a format that carries alpha,
//  e.g. a PNG. This test verifies the opaque path only.)
static void Write2x1Bmp(const std::string& path)
{
    // Row must be padded to a 4-byte boundary: 2 pixels × 3 bytes = 6 → pad to 8 bytes
    const uint8_t bmp[62] = {
        'B', 'M',
        62,  0,   0,   0,
        0,   0,   0,   0,
        54,  0,   0,   0,
        40,  0,   0,   0,
        2,   0,   0,   0,   // width = 2
        1,   0,   0,   0,   // height = 1
        1,   0,
        24,  0,
        0,   0,   0,   0,
        8,   0,   0,   0,   // image size = 8 (row = 6 + 2 pad)
        19,  11,  0,   0,
        19,  11,  0,   0,
        0,   0,   0,   0,
        0,   0,   0,   0,
        // pixel data: BGR BGR + 2 pad bytes
        255, 255, 255,       // pixel 0 — white
        0,   0,   0,         // pixel 1 — black
        0,   0               // row padding
    };
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(bmp), sizeof(bmp));
}

// =============================================================================
// Fixture
// =============================================================================

class t_ImporterTexture : public ::testing::Test
{
protected:
    void SetUp() override
    {
        nous::engine::memory::InitializeMemory(MiB(16));
        m_bmpPath = (fs::temp_directory_path() / "nous_t_importer_texture.bmp").string();
        m_bmp2Path = (fs::temp_directory_path() / "nous_t_importer_texture2.bmp").string();
    }

    void TearDown() override
    {
        std::error_code ec;
        fs::remove(m_bmpPath,  ec);
        fs::remove(m_bmp2Path, ec);
        nous::engine::memory::ShutdownMemory();
    }

    std::string m_bmpPath;
    std::string m_bmp2Path;
};

// =============================================================================
// Deserialize — error cases
// =============================================================================

TEST_F(t_ImporterTexture, Deserialize_NonexistentFile_ReturnsFalse)
{
    ResourceTexture tex;
    ImporterTexture importer;
    EXPECT_FALSE(importer.Deserialize("__nonexistent__.bmp", &tex));
}

// =============================================================================
// Deserialize — success path
// =============================================================================

TEST_F(t_ImporterTexture, Deserialize_ValidBmp_ReturnsTrue)
{
    WriteMinimalBmp(m_bmpPath, 255, 0, 0);
    ResourceTexture tex;
    ImporterTexture importer;
    EXPECT_TRUE(importer.Deserialize(m_bmpPath, &tex));
}

TEST_F(t_ImporterTexture, Deserialize_ValidBmp_SetsWidthAndHeight)
{
    WriteMinimalBmp(m_bmpPath, 255, 0, 0);
    ResourceTexture tex;
    ImporterTexture importer;
    ASSERT_TRUE(importer.Deserialize(m_bmpPath, &tex));
    EXPECT_EQ(tex.width,  1u);
    EXPECT_EQ(tex.height, 1u);
}

TEST_F(t_ImporterTexture, Deserialize_ValidBmp_ChannelCountIsFour)
{
    // ImporterTexture always forces RGBA (requiredChannelCount = 4)
    WriteMinimalBmp(m_bmpPath, 0, 255, 0);
    ResourceTexture tex;
    ImporterTexture importer;
    ASSERT_TRUE(importer.Deserialize(m_bmpPath, &tex));
    EXPECT_EQ(tex.channelCount, 4u);
}

TEST_F(t_ImporterTexture, Deserialize_ValidBmp_PixelDataIsNonEmpty)
{
    WriteMinimalBmp(m_bmpPath, 0, 0, 255);
    ResourceTexture tex;
    ImporterTexture importer;
    ASSERT_TRUE(importer.Deserialize(m_bmpPath, &tex));
    EXPECT_FALSE(tex.pixelData.empty());
    EXPECT_EQ(tex.pixelData.size(), 1u * 1u * 4u); // w × h × 4
}

TEST_F(t_ImporterTexture, Deserialize_ValidBmp_GenerationIsZero)
{
    WriteMinimalBmp(m_bmpPath, 128, 128, 128);
    ResourceTexture tex;
    ImporterTexture importer;
    ASSERT_TRUE(importer.Deserialize(m_bmpPath, &tex));
    EXPECT_EQ(tex.generation, 0u);
}

TEST_F(t_ImporterTexture, Deserialize_OpaqueImage_HasTransparencyIsFalse)
{
    WriteMinimalBmp(m_bmpPath, 255, 255, 255);
    ResourceTexture tex;
    ImporterTexture importer;
    ASSERT_TRUE(importer.Deserialize(m_bmpPath, &tex));
    // BMP has no alpha channel; stb_image fills forced alpha=255 → not transparent
    EXPECT_FALSE(tex.hasTransparency);
}

TEST_F(t_ImporterTexture, Deserialize_2x1Bmp_SetsCorrectDimensions)
{
    Write2x1Bmp(m_bmp2Path);
    ResourceTexture tex;
    ImporterTexture importer;
    ASSERT_TRUE(importer.Deserialize(m_bmp2Path, &tex));
    EXPECT_EQ(tex.width,  2u);
    EXPECT_EQ(tex.height, 1u);
    EXPECT_EQ(tex.pixelData.size(), 2u * 1u * 4u);
}

// =============================================================================
// Evict
// =============================================================================

TEST_F(t_ImporterTexture, Evict_ClearsPixelData)
{
    WriteMinimalBmp(m_bmpPath, 100, 100, 100);
    ResourceTexture tex;
    ImporterTexture importer;
    ASSERT_TRUE(importer.Deserialize(m_bmpPath, &tex));
    ASSERT_FALSE(tex.pixelData.empty());

    importer.Evict(&tex);
    EXPECT_TRUE(tex.pixelData.empty());
    EXPECT_EQ(tex.pixelData.capacity(), 0u);
}
