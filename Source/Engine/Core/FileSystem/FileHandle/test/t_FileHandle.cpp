#include <gtest/gtest.h>

#include "Engine/Core/FileSystem/FileHandle/include/FileHandle.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/Core/Globals.h"

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

class t_FileHandle : public ::testing::Test
{
protected:
    void SetUp() override
    {
        MemoryManager::InitializeMemory(MiB(4));
        m_path = (fs::temp_directory_path() / "nous_t_filehandle.tmp").string();
    }

    void TearDown() override
    {
        std::error_code ec;
        fs::remove(m_path, ec);
        MemoryManager::ShutdownMemory();
    }

    std::string m_path;
};

// =============================================================================
// Open / Close / IsOpen
// =============================================================================

TEST_F(t_FileHandle, Open_NonexistentFile_ForRead_ReturnsFalse)
{
    FileHandle fh;
    EXPECT_FALSE(fh.Open("__nonexistent_file__.tmp", FileMode::READ, false));
    EXPECT_FALSE(fh.IsOpen());
}

TEST_F(t_FileHandle, Open_ForWrite_CreatesFile)
{
    FileHandle fh;
    ASSERT_TRUE(fh.Open(m_path, FileMode::WRITE, false));
    EXPECT_TRUE(fh.IsOpen());
    fh.Close();
    EXPECT_TRUE(fs::exists(m_path));
}

TEST_F(t_FileHandle, Close_SetsIsOpenToFalse)
{
    FileHandle fh;
    ASSERT_TRUE(fh.Open(m_path, FileMode::WRITE, false));
    fh.Close();
    EXPECT_FALSE(fh.IsOpen());
}

TEST_F(t_FileHandle, SetPath_GetPath_Roundtrip)
{
    FileHandle fh;
    fh.SetPath("some/path.txt");
    EXPECT_EQ(fh.GetPath(), "some/path.txt");
}

// =============================================================================
// Write / ReadAllBytes
// =============================================================================

TEST_F(t_FileHandle, WriteAndReadAllBytes_Roundtrip)
{
    const std::string payload = "Hello, Nous Engine!";

    {
        FileHandle fh;
        ASSERT_TRUE(fh.Open(m_path, FileMode::WRITE, true));
        uint64 written = 0;
        ASSERT_TRUE(fh.Write(payload.size(), payload.data(), &written));
        EXPECT_EQ(written, payload.size());
        fh.Close();
    }

    {
        FileHandle fh;
        ASSERT_TRUE(fh.Open(m_path, FileMode::READ, true));
        char*  buffer    = nullptr;
        uint64 bytesRead = 0;
        ASSERT_TRUE(fh.ReadAllBytes(&buffer, &bytesRead));
        ASSERT_NE(buffer, nullptr);
        EXPECT_EQ(bytesRead, payload.size());
        EXPECT_EQ(std::string(buffer, bytesRead), payload);
        NOUS_DELETE_ARRAY(buffer, bytesRead, MemoryTag::FILE);
        fh.Close();
    }
}

TEST_F(t_FileHandle, ReadBytes_ReadsSubset)
{
    const std::string payload = "ABCDEFGH";

    {
        FileHandle fh;
        ASSERT_TRUE(fh.Open(m_path, FileMode::WRITE, true));
        uint64 w = 0;
        fh.Write(payload.size(), payload.data(), &w);
        fh.Close();
    }

    {
        FileHandle fh;
        ASSERT_TRUE(fh.Open(m_path, FileMode::READ, true));
        char   buf[4]    = {};
        uint64 bytesRead = 0;
        ASSERT_TRUE(fh.ReadBytes(4, buf, &bytesRead));
        EXPECT_EQ(bytesRead, 4u);
        EXPECT_EQ(std::string(buf, 4), "ABCD");
        fh.Close();
    }
}

// =============================================================================
// WriteLine / ReadLine
// =============================================================================

TEST_F(t_FileHandle, WriteLineAndReadLine_Roundtrip)
{
    const std::string line = "The quick brown fox";

    {
        FileHandle fh;
        ASSERT_TRUE(fh.Open(m_path, FileMode::WRITE, false));
        ASSERT_TRUE(fh.WriteLine(line));
        fh.Close();
    }

    {
        FileHandle fh;
        ASSERT_TRUE(fh.Open(m_path, FileMode::READ, false));
        std::string read;
        ASSERT_TRUE(fh.ReadLine(read));
        EXPECT_EQ(read, line);
        fh.Close();
    }
}

TEST_F(t_FileHandle, ReadLine_AtEOF_ReturnsFalse)
{
    {
        FileHandle fh;
        ASSERT_TRUE(fh.Open(m_path, FileMode::WRITE, false));
        fh.WriteLine("OnlyLine");
        fh.Close();
    }

    {
        FileHandle fh;
        ASSERT_TRUE(fh.Open(m_path, FileMode::READ, false));
        std::string line;
        fh.ReadLine(line);
        EXPECT_FALSE(fh.ReadLine(line));
        fh.Close();
    }
}

// =============================================================================
// Binary write — verifies byte-exact output
// =============================================================================

TEST_F(t_FileHandle, BinaryWrite_PreservesExactBytes)
{
    const uint8_t bytes[] = { 0x00, 0xFF, 0xAB, 0x42 };

    {
        FileHandle fh;
        ASSERT_TRUE(fh.Open(m_path, FileMode::WRITE, true));
        uint64 w = 0;
        fh.Write(sizeof(bytes), bytes, &w);
        fh.Close();
    }

    {
        FileHandle fh;
        ASSERT_TRUE(fh.Open(m_path, FileMode::READ, true));
        char*  buf   = nullptr;
        uint64 bread = 0;
        ASSERT_TRUE(fh.ReadAllBytes(&buf, &bread));
        ASSERT_EQ(bread, sizeof(bytes));
        for (int i = 0; i < static_cast<int>(sizeof(bytes)); ++i)
            EXPECT_EQ(static_cast<uint8_t>(buf[i]), bytes[i]) << "Byte " << i;
        NOUS_DELETE_ARRAY(buf, bread, MemoryTag::FILE);
        fh.Close();
    }
}
