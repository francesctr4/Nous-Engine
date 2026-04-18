#include <gtest/gtest.h>

#include "Engine/Core/FileSystem/FileSystem.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

// =============================================================================
// Pure string operations — no fixture needed
// =============================================================================

TEST(t_FileSystem, NormalizePath_ReplacesBackslashesWithForwardSlashes)
{
    const std::string result = NOUS_FileManager::NormalizePath("Assets\\Shaders\\test.glsl");
    EXPECT_EQ(result, "Assets/Shaders/test.glsl");
}

TEST(t_FileSystem, NormalizePath_ForwardSlashPath_Unchanged)
{
    const std::string result = NOUS_FileManager::NormalizePath("Assets/Shaders/test.glsl");
    EXPECT_EQ(result, "Assets/Shaders/test.glsl");
}

TEST(t_FileSystem, GetFilename_ReturnsBaseNameWithoutExtension)
{
    // stem() strips the extension
    EXPECT_EQ(NOUS_FileManager::GetFilename("Assets/Shaders/test.glsl"), "test");
}

TEST(t_FileSystem, GetFilename_FileWithMultipleDots)
{
    EXPECT_EQ(NOUS_FileManager::GetFilename("BuiltIn.MaterialShader.vert.glsl"), "BuiltIn.MaterialShader.vert");
}

TEST(t_FileSystem, GetExtension_ReturnsExtensionWithDot)
{
    EXPECT_EQ(NOUS_FileManager::GetExtension("test.glsl"), ".glsl");
}

TEST(t_FileSystem, GetExtension_NoExtension_ReturnsEmpty)
{
    EXPECT_EQ(NOUS_FileManager::GetExtension("noextension"), "");
}

TEST(t_FileSystem, GetExtension_DotFile_ReturnsFull)
{
    // ".cpp" has no stem — extension() returns the whole token
    // behaviour matches std::filesystem::path::extension()
    EXPECT_EQ(NOUS_FileManager::GetExtension("file.cpp"), ".cpp");
}

// =============================================================================
// Fixture — uses a temp directory created fresh per test
// =============================================================================

class t_FileSystemOps : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_tempDir = (fs::temp_directory_path() / "nous_t_filesystem").string();
        fs::create_directories(m_tempDir);
    }

    void TearDown() override
    {
        std::error_code ec;
        fs::remove_all(m_tempDir, ec);
    }

    std::string m_tempDir;
};

TEST_F(t_FileSystemOps, Exists_ReturnsTrueForExistingDirectory)
{
    EXPECT_TRUE(NOUS_FileManager::Exists(m_tempDir));
}

TEST_F(t_FileSystemOps, Exists_ReturnsFalseForNonExistentPath)
{
    EXPECT_FALSE(NOUS_FileManager::Exists(m_tempDir + "/does_not_exist"));
}

TEST_F(t_FileSystemOps, IsDirectory_TrueForDirectory)
{
    EXPECT_TRUE(NOUS_FileManager::IsDirectory(m_tempDir));
}

TEST_F(t_FileSystemOps, IsDirectory_FalseForFile)
{
    const std::string filePath = m_tempDir + "/sample.txt";
    { std::ofstream f(filePath); f << "data"; }
    EXPECT_FALSE(NOUS_FileManager::IsDirectory(filePath));
}

TEST_F(t_FileSystemOps, CreateDirectory_CreatesNewDir)
{
    const std::string newDir = m_tempDir + "/subdir";
    EXPECT_TRUE(NOUS_FileManager::CreateDirectory(newDir));
    EXPECT_TRUE(fs::exists(newDir));
}

TEST_F(t_FileSystemOps, CreateDirectory_AlreadyExists_ReturnsTrue)
{
    EXPECT_TRUE(NOUS_FileManager::CreateDirectory(m_tempDir));
}

TEST_F(t_FileSystemOps, DeleteDirectory_RemovesDirectory)
{
    const std::string subDir = m_tempDir + "/to_delete";
    fs::create_directories(subDir);
    ASSERT_TRUE(fs::exists(subDir));
    EXPECT_TRUE(NOUS_FileManager::DeleteDirectory(subDir));
    EXPECT_FALSE(fs::exists(subDir));
}

TEST_F(t_FileSystemOps, DeleteFile_RemovesFile)
{
    const std::string filePath = m_tempDir + "/to_delete.txt";
    { std::ofstream f(filePath); f << "x"; }
    ASSERT_TRUE(fs::exists(filePath));
    EXPECT_TRUE(NOUS_FileManager::DeleteFile(filePath));
    EXPECT_FALSE(fs::exists(filePath));
}

TEST_F(t_FileSystemOps, DeleteFile_NonExistentFile_ReturnsFalse)
{
    EXPECT_FALSE(NOUS_FileManager::DeleteFile(m_tempDir + "/ghost.txt"));
}

TEST_F(t_FileSystemOps, CopyFile_CreatesCopyAtDestination)
{
    const std::string src = m_tempDir + "/src.txt";
    const std::string dst = m_tempDir + "/dst.txt";
    { std::ofstream f(src); f << "hello"; }

    EXPECT_TRUE(NOUS_FileManager::CopyFile(src, dst));
    EXPECT_TRUE(fs::exists(dst));
}
