#include <gtest/gtest.h>

#include <FileSystem/FileHandle.h>
#include <MemoryManager/MemoryManager.h>
#include <Utils/DataStructures/NOUS_Vector.h>

#include <filesystem>
#include <optional>
#include <span>
#include <string>

namespace fs = std::filesystem;

class t_FileHandle : public ::testing::Test
{
protected:
    void SetUp() override
    {
        nous::engine::memory::InitializeMemory(MiB(4));
        m_path = (fs::temp_directory_path() / "nous_t_filehandle.tmp").string();
    }

    void TearDown() override
    {
        std::error_code ec;
        fs::remove(m_path, ec);
        nous::engine::memory::ShutdownMemory();
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
        const std::optional<uint64_t> written = fh.Write(std::as_bytes(std::span(payload)));
        ASSERT_TRUE(written.has_value());
        EXPECT_EQ(*written, payload.size());
        fh.Close();
    }

    {
        FileHandle fh;
        ASSERT_TRUE(fh.Open(m_path, FileMode::READ, true));

        std::optional<NOUS_Vector<char>> buffer = fh.ReadAllBytes();

        ASSERT_TRUE(buffer.has_value());
        EXPECT_EQ(buffer->size(), payload.size());
        EXPECT_EQ(std::string(buffer->data(), buffer->size()), payload);
        fh.Close();
    }
}

TEST_F(t_FileHandle, ReadAllBytes_OnClosedHandle_ReturnsNullopt)
{
    FileHandle fh;
    EXPECT_FALSE(fh.ReadAllBytes().has_value());
}

TEST_F(t_FileHandle, ReadAllBytes_OnEmptyFile_ReturnsNullopt)
{
    {
        FileHandle fh;
        ASSERT_TRUE(fh.Open(m_path, FileMode::WRITE, true));
        fh.Close();
    }

    FileHandle fh;
    ASSERT_TRUE(fh.Open(m_path, FileMode::READ, true));
    // Preserves today's behavior: the "file is empty" guard rejects before allocating.
    EXPECT_FALSE(fh.ReadAllBytes().has_value());
    fh.Close();
}

TEST_F(t_FileHandle, ReadAllBytes_OnWriteOnlyHandle_ReturnsNullopt)
{
    FileHandle fh;
    ASSERT_TRUE(fh.Open(m_path, FileMode::WRITE, true));
    EXPECT_FALSE(fh.ReadAllBytes().has_value());
    fh.Close();
}

TEST_F(t_FileHandle, ReadBytes_ReadsSubset)
{
    const std::string payload = "ABCDEFGH";

    {
        FileHandle fh;
        ASSERT_TRUE(fh.Open(m_path, FileMode::WRITE, true));
        fh.Write(std::as_bytes(std::span(payload)));
        fh.Close();
    }

    {
        FileHandle fh;
        ASSERT_TRUE(fh.Open(m_path, FileMode::READ, true));
        char buf[4] = {};
        const std::optional<uint64_t> bytesRead = fh.ReadBytes(std::span(buf));
        ASSERT_TRUE(bytesRead.has_value());
        EXPECT_EQ(*bytesRead, 4u);
        EXPECT_EQ(std::string(buf, 4), "ABCD");
        fh.Close();
    }
}

TEST_F(t_FileHandle, ReadBytes_ReadsRequestedSubset)
{
    const std::string payload = "ABCDEFGH";

    {
        FileHandle fh;
        ASSERT_TRUE(fh.Open(m_path, FileMode::WRITE, true));
        ASSERT_TRUE(fh.Write(std::as_bytes(std::span(payload))).has_value());
        fh.Close();
    }

    FileHandle fh;
    ASSERT_TRUE(fh.Open(m_path, FileMode::READ, true));

    char dst[4] = {};
    const std::optional<uint64_t> read = fh.ReadBytes(std::span(dst));

    ASSERT_TRUE(read.has_value());
    EXPECT_EQ(*read, 4u);
    EXPECT_EQ(std::string(dst, 4), "ABCD");
    fh.Close();
}

TEST_F(t_FileHandle, ReadBytes_AtEndOfFile_ReturnsNullopt)
{
    const std::string payload = "AB";

    {
        FileHandle fh;
        ASSERT_TRUE(fh.Open(m_path, FileMode::WRITE, true));
        ASSERT_TRUE(fh.Write(std::as_bytes(std::span(payload))).has_value());
        fh.Close();
    }

    FileHandle fh;
    ASSERT_TRUE(fh.Open(m_path, FileMode::READ, true));

    char drain[2] = {};
    ASSERT_TRUE(fh.ReadBytes(std::span(drain)).has_value());

    // Zero bytes transferred must be nullopt, NOT a present optional holding 0.
    // ImporterMesh.cpp:258 and :289 branch on this; treating it as success
    // would silently accept truncated .nmesh files.
    char past[2] = {};
    EXPECT_FALSE(fh.ReadBytes(std::span(past)).has_value());
    fh.Close();
}

TEST_F(t_FileHandle, ReadBytes_ShortRead_ReturnsActualCount)
{
    const std::string payload = "ABC";

    {
        FileHandle fh;
        ASSERT_TRUE(fh.Open(m_path, FileMode::WRITE, true));
        ASSERT_TRUE(fh.Write(std::as_bytes(std::span(payload))).has_value());
        fh.Close();
    }

    FileHandle fh;
    ASSERT_TRUE(fh.Open(m_path, FileMode::READ, true));

    char dst[8] = {};
    const std::optional<uint64_t> read = fh.ReadBytes(std::span(dst));

    // Fewer bytes than requested is a warning, not a failure.
    ASSERT_TRUE(read.has_value());
    EXPECT_EQ(*read, 3u);
    fh.Close();
}

TEST_F(t_FileHandle, Write_ReturnsBytesWritten)
{
    const std::string payload = "written";

    FileHandle fh;
    ASSERT_TRUE(fh.Open(m_path, FileMode::WRITE, true));

    const std::optional<uint64_t> written = fh.Write(std::as_bytes(std::span(payload)));

    ASSERT_TRUE(written.has_value());
    EXPECT_EQ(*written, payload.size());
    fh.Close();
}

TEST_F(t_FileHandle, Write_EmptySpan_DoesNotReportFailure)
{
    // Unlike ReadBytes, a zero-byte Write is not a failure — the old
    // `Write` only ever gated on fileStream->fail(), never on the byte
    // count. ImporterMesh.cpp writes a 0-byte embedded texture in the
    // degenerate case and must not see that as an error.
    FileHandle fh;
    ASSERT_TRUE(fh.Open(m_path, FileMode::WRITE, true));

    const std::span<const std::byte> empty;
    const std::optional<uint64_t> written = fh.Write(empty);

    EXPECT_TRUE(written.has_value());
    fh.Close();
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
        fh.Write(std::as_bytes(std::span(bytes)));
        fh.Close();
    }

    {
        FileHandle fh;
        ASSERT_TRUE(fh.Open(m_path, FileMode::READ, true));

        std::optional<NOUS_Vector<char>> buffer = fh.ReadAllBytes();

        ASSERT_TRUE(buffer.has_value());
        ASSERT_EQ(buffer->size(), sizeof(bytes));
        for (int i = 0; i < static_cast<int>(sizeof(bytes)); ++i)
            EXPECT_EQ(static_cast<uint8_t>((*buffer)[i]), bytes[i]) << "Byte " << i;
        fh.Close();
    }
}
