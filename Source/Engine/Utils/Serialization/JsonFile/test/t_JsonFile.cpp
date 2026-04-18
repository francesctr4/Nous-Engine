#include <gtest/gtest.h>

#include "Engine/Utils/Serialization/JsonFile/JsonFile.h"

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

// =============================================================================
// Helpers
// =============================================================================

namespace
{
    const std::string kTempPath =
        (fs::temp_directory_path() / "nous_t_jsonfile.json").string();
}

// =============================================================================
// AppendValue / GetValue roundtrip
// =============================================================================

TEST(t_JsonFile, AppendAndGet_Int_Roundtrip)
{
    JsonFile jf;
    jf.AppendValue("myInt", 42);
    int out = 0;
    ASSERT_TRUE(jf.GetValue("myInt", out));
    EXPECT_EQ(out, 42);
}

TEST(t_JsonFile, AppendAndGet_Float_Roundtrip)
{
    JsonFile jf;
    jf.AppendValue("myFloat", 3.14f);
    float out = 0.0f;
    ASSERT_TRUE(jf.GetValue("myFloat", out));
    EXPECT_NEAR(out, 3.14f, 1e-4f);
}

TEST(t_JsonFile, AppendAndGet_Double_Roundtrip)
{
    JsonFile jf;
    jf.AppendValue("myDouble", 2.718281828);
    double out = 0.0;
    ASSERT_TRUE(jf.GetValue("myDouble", out));
    EXPECT_NEAR(out, 2.718281828, 1e-9);
}

TEST(t_JsonFile, AppendAndGet_String_Roundtrip)
{
    JsonFile jf;
    jf.AppendValue("myStr", std::string("hello"));
    std::string out;
    ASSERT_TRUE(jf.GetValue("myStr", out));
    EXPECT_EQ(out, "hello");
}

TEST(t_JsonFile, AppendAndGet_CString_Roundtrip)
{
    JsonFile jf;
    jf.AppendValue("cstr", "world");
    std::string out;
    ASSERT_TRUE(jf.GetValue("cstr", out));
    EXPECT_EQ(out, "world");
}

TEST(t_JsonFile, AppendAndGet_BoolTrue_Roundtrip)
{
    JsonFile jf;
    jf.AppendValue("flag", true);
    bool out = false;
    ASSERT_TRUE(jf.GetValue("flag", out));
    EXPECT_TRUE(out);
}

TEST(t_JsonFile, AppendAndGet_BoolFalse_Roundtrip)
{
    JsonFile jf;
    jf.AppendValue("flag", false);
    bool out = true;
    ASSERT_TRUE(jf.GetValue("flag", out));
    EXPECT_FALSE(out);
}

TEST(t_JsonFile, GetValue_MissingKey_ReturnsFalse)
{
    JsonFile jf;
    int out = 0;
    EXPECT_FALSE(jf.GetValue("nonexistent", out));
}

TEST(t_JsonFile, MultipleValues_AllRetrievable)
{
    JsonFile jf;
    jf.AppendValue("a", 1);
    jf.AppendValue("b", 2);
    jf.AppendValue("c", 3);
    int a = 0, b = 0, c = 0;
    EXPECT_TRUE(jf.GetValue("a", a));
    EXPECT_TRUE(jf.GetValue("b", b));
    EXPECT_TRUE(jf.GetValue("c", c));
    EXPECT_EQ(a, 1);
    EXPECT_EQ(b, 2);
    EXPECT_EQ(c, 3);
}

// =============================================================================
// SaveToFile / LoadFromFile roundtrip
// =============================================================================

TEST(t_JsonFile, SaveAndLoad_PreservesInt)
{
    {
        JsonFile jf;
        jf.AppendValue("x", 99);
        ASSERT_TRUE(jf.SaveToFile(kTempPath.c_str()));
    }
    {
        JsonFile jf;
        ASSERT_TRUE(jf.LoadFromFile(kTempPath.c_str()));
        int out = 0;
        ASSERT_TRUE(jf.GetValue("x", out));
        EXPECT_EQ(out, 99);
    }
    std::error_code ec;
    fs::remove(kTempPath, ec);
}

TEST(t_JsonFile, SaveAndLoad_PreservesString)
{
    {
        JsonFile jf;
        jf.AppendValue("name", std::string("engine"));
        ASSERT_TRUE(jf.SaveToFile(kTempPath.c_str()));
    }
    {
        JsonFile jf;
        ASSERT_TRUE(jf.LoadFromFile(kTempPath.c_str()));
        std::string out;
        ASSERT_TRUE(jf.GetValue("name", out));
        EXPECT_EQ(out, "engine");
    }
    std::error_code ec;
    fs::remove(kTempPath, ec);
}

TEST(t_JsonFile, LoadFromFile_NonExistent_ReturnsFalse)
{
    JsonFile jf;
    EXPECT_FALSE(jf.LoadFromFile("__no_such_file__.json"));
}
