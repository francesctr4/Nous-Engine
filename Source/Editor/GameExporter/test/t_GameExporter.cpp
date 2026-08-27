#include <gtest/gtest.h>
#include <ModuleEditor/GameExporter.h>

#include <filesystem>
#include <fstream>

using namespace GameExporterPlatform;

// ─── Platform helpers ─────────────────────────────────────────────────────────

TEST(PlatformHelpers, ExeExtension)
{
#ifdef _WIN32
    EXPECT_EQ(GetExeExtension(), ".exe");
#else
    EXPECT_EQ(GetExeExtension(), "");
#endif
}

TEST(PlatformHelpers, SharedLibExt)
{
#ifdef _WIN32
    EXPECT_EQ(GetSharedLibExt(), ".dll");
#elif defined(__APPLE__)
    EXPECT_EQ(GetSharedLibExt(), ".dylib");
#else
    EXPECT_EQ(GetSharedLibExt(), ".so");
#endif
}

TEST(PlatformHelpers, ScriptLibName)
{
#ifdef _WIN32
    EXPECT_EQ(GetScriptLibName(), "Scripts.dll");
#elif defined(__APPLE__)
    EXPECT_EQ(GetScriptLibName(), "Scripts.dylib");
#else
    EXPECT_EQ(GetScriptLibName(), "Scripts.so");
#endif
}

// ─── CopySharedLibs ───────────────────────────────────────────────────────────

TEST(CopySharedLibs, OnlyCopiesPlatformExt)
{
    namespace fs = std::filesystem;
    const auto tmpBase = fs::temp_directory_path() / "nous_test_copylibs";
    fs::remove_all(tmpBase);
    const auto src = tmpBase / "src";
    const auto dst = tmpBase / "dst";
    fs::create_directories(src);

    for (const auto* ext : {".dll", ".so", ".dylib", ".txt"})
        std::ofstream(src / (std::string("lib") + ext)).close();

    std::error_code ec;
    ASSERT_TRUE(CopySharedLibs(src, dst, ec)) << ec.message();

    int count = 0;
    for (const auto& e : fs::directory_iterator(dst))
        ++count;
    EXPECT_EQ(count, 1);
    EXPECT_TRUE(fs::exists(dst / (std::string("lib") + std::string(GetSharedLibExt()))));

    fs::remove_all(tmpBase);
}

TEST(CopySharedLibs, CreatesDestDir)
{
    namespace fs = std::filesystem;
    const auto tmpBase = fs::temp_directory_path() / "nous_test_createdir";
    fs::remove_all(tmpBase);
    const auto src = tmpBase / "src";
    const auto dst = tmpBase / "dst" / "nested";
    fs::create_directories(src);
    std::ofstream(src / (std::string("lib") + std::string(GetSharedLibExt()))).close();

    std::error_code ec;
    ASSERT_TRUE(CopySharedLibs(src, dst, ec)) << ec.message();
    EXPECT_TRUE(fs::exists(dst));

    fs::remove_all(tmpBase);
}

// ─── PatchGameConfig ──────────────────────────────────────────────────────────

TEST(PatchGameConfig, SetsStartScene)
{
    namespace fs = std::filesystem;
    const auto tmp = fs::temp_directory_path() / "nous_test_patchcfg.json";
    {
        std::ofstream f(tmp);
        f << R"({"startScene":"old_scene.nous","windowTitle":"Test"})";
    }
    ASSERT_TRUE(PatchGameConfig(tmp, "new_scene.nous"));

    std::string contents;
    {
        std::ifstream f(tmp);
        contents = std::string((std::istreambuf_iterator<char>(f)), {});
    }
    EXPECT_NE(contents.find("new_scene.nous"), std::string::npos);
    EXPECT_EQ(contents.find("old_scene.nous"), std::string::npos);

    fs::remove(tmp);
}

TEST(PatchGameConfig, ReturnsFalseOnMissingFile)
{
    const std::filesystem::path nonexistent =
        std::filesystem::temp_directory_path() / "nous_does_not_exist_99999.json";
    EXPECT_FALSE(PatchGameConfig(nonexistent, "scene.nous"));
}
