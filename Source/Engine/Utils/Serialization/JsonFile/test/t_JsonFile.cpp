#include <gtest/gtest.h>
#include <Engine/Utils/Serialization/JsonFile/JsonObject.h>
#include <Engine/Utils/Serialization/JsonFile/JsonArray.h>
#include <Engine/Utils/Serialization/JsonFile/JsonFile.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <filesystem>

// --- JsonObject: scalar round-trip ---

TEST(JsonObject, SetGetInt) {
    JsonObject obj;
    obj.Set("x", 42);
    EXPECT_EQ(obj.GetInt("x"), 42);
}

TEST(JsonObject, SetGetFloat) {
    JsonObject obj;
    obj.Set("v", 3.14f);
    EXPECT_NEAR(obj.GetFloat("v"), 3.14f, 1e-5f);
}

TEST(JsonObject, SetGetDouble) {
    JsonObject obj;
    obj.Set("d", 1.23456789);
    EXPECT_NEAR(obj.GetDouble("d"), 1.23456789, 1e-9);
}

TEST(JsonObject, SetGetBool) {
    JsonObject obj;
    obj.Set("flag", true);
    EXPECT_TRUE(obj.GetBool("flag"));
    obj.Set("other", false);
    EXPECT_FALSE(obj.GetBool("other"));
}

TEST(JsonObject, SetGetString) {
    JsonObject obj;
    obj.Set("name", std::string_view("hello"));
    EXPECT_EQ(obj.GetString("name"), "hello");
}

TEST(JsonObject, MissingKeyReturnsDefault) {
    JsonObject obj;
    EXPECT_EQ(obj.GetInt("missing"), 0);
    EXPECT_NEAR(obj.GetFloat("missing"), 0.f, 1e-6f);
    EXPECT_EQ(obj.GetString("missing"), "");
    EXPECT_FALSE(obj.GetBool("missing"));
    EXPECT_TRUE(obj.GetObject("missing").IsEmpty());
    EXPECT_TRUE(obj.GetArray("missing").IsEmpty());
}

// --- JsonObject: GLM types ---

TEST(JsonObject, SetGetVec3) {
    JsonObject obj;
    obj.Set("pos", glm::vec3(1.f, 2.f, 3.f));
    const glm::vec3 v = obj.GetVec3("pos");
    EXPECT_NEAR(v.x, 1.f, 1e-5f);
    EXPECT_NEAR(v.y, 2.f, 1e-5f);
    EXPECT_NEAR(v.z, 3.f, 1e-5f);
}

TEST(JsonObject, SetGetVec4) {
    JsonObject obj;
    obj.Set("col", glm::vec4(0.1f, 0.2f, 0.3f, 1.f));
    const glm::vec4 v = obj.GetVec4("col");
    EXPECT_NEAR(v.r, 0.1f, 1e-5f);
    EXPECT_NEAR(v.g, 0.2f, 1e-5f);
    EXPECT_NEAR(v.b, 0.3f, 1e-5f);
    EXPECT_NEAR(v.a, 1.f,  1e-5f);
}

TEST(JsonObject, SetGetQuat) {
    JsonObject obj;
    const glm::quat q = glm::quat(1.f, 0.f, 0.f, 0.f);  // identity: w=1
    obj.Set("rot", q);
    const glm::quat r = obj.GetQuat("rot");
    EXPECT_NEAR(r.w, 1.f, 1e-5f);
    EXPECT_NEAR(r.x, 0.f, 1e-5f);
    EXPECT_NEAR(r.y, 0.f, 1e-5f);
    EXPECT_NEAR(r.z, 0.f, 1e-5f);
}

// --- JsonObject: nested children ---

TEST(JsonObject, SetGetNestedObject) {
    JsonObject parent;
    JsonObject child;
    child.Set("val", 99);
    parent.Set("child", std::move(child));
    EXPECT_TRUE(child.IsEmpty());  // ownership transferred

    const JsonObject retrieved = parent.GetObject("child");
    EXPECT_EQ(retrieved.GetInt("val"), 99);
}

TEST(JsonObject, SetGetNestedArray) {
    JsonObject obj;
    JsonArray arr;
    arr.Append(10);
    arr.Append(20);
    obj.Set("arr", std::move(arr));
    EXPECT_TRUE(arr.IsEmpty());

    const JsonArray retrieved = obj.GetArray("arr");
    EXPECT_EQ(retrieved.Count(), 2);
    EXPECT_EQ(retrieved.GetInt(0), 10);
    EXPECT_EQ(retrieved.GetInt(1), 20);
}

// --- JsonArray ---

TEST(JsonArray, AppendAndGetInt) {
    JsonArray arr;
    arr.Append(7);
    arr.Append(13);
    EXPECT_EQ(arr.Count(), 2);
    EXPECT_EQ(arr.GetInt(0), 7);
    EXPECT_EQ(arr.GetInt(1), 13);
}

TEST(JsonArray, AppendAndGetObject) {
    JsonArray arr;
    JsonObject obj;
    obj.Set("id", 1);
    arr.Append(std::move(obj));
    EXPECT_EQ(arr.Count(), 1);
    EXPECT_EQ(arr.GetObject(0).GetInt("id"), 1);
}

TEST(JsonArray, Remove) {
    JsonArray arr;
    arr.Append(1);
    arr.Append(2);
    arr.Append(3);
    arr.Remove(1);
    EXPECT_EQ(arr.Count(), 2);
    EXPECT_EQ(arr.GetInt(0), 1);
    EXPECT_EQ(arr.GetInt(1), 3);
}

// --- JsonFile round-trip ---

TEST(JsonFile, SaveAndLoadScalars) {
    const std::string path = "test_scalars.json";
    {
        JsonObject obj;
        obj.Set("n", 42);
        obj.Set("s", std::string_view("hello"));
        obj.Set("b", true);
        EXPECT_TRUE(JsonFile::SaveToFile(obj, path));
    }
    const JsonObject loaded = JsonFile::LoadFromFile(path);
    EXPECT_EQ(loaded.GetInt("n"), 42);
    EXPECT_EQ(loaded.GetString("s"), "hello");
    EXPECT_TRUE(loaded.GetBool("b"));
    std::filesystem::remove(path);
}

TEST(JsonFile, SaveAndLoadGLM) {
    const std::string path = "test_glm.json";
    {
        JsonObject obj;
        obj.Set("pos", glm::vec3(1.f, 2.f, 3.f));
        obj.Set("rot", glm::quat(1.f, 0.f, 0.f, 0.f));
        EXPECT_TRUE(JsonFile::SaveToFile(obj, path));
    }
    const JsonObject loaded = JsonFile::LoadFromFile(path);
    const glm::vec3 pos = loaded.GetVec3("pos");
    EXPECT_NEAR(pos.x, 1.f, 1e-5f);
    const glm::quat rot = loaded.GetQuat("rot");
    EXPECT_NEAR(rot.w, 1.f, 1e-5f);
    std::filesystem::remove(path);
}

TEST(JsonFile, SaveAndLoadNestedArray) {
    const std::string path = "test_nested.json";
    {
        JsonObject root;
        JsonArray items;
        for (int i = 0; i < 3; ++i) {
            JsonObject item;
            item.Set("id", i);
            items.Append(std::move(item));
        }
        root.Set("items", std::move(items));
        EXPECT_TRUE(JsonFile::SaveToFile(root, path));
    }
    const JsonObject loaded = JsonFile::LoadFromFile(path);
    const JsonArray items = loaded.GetArray("items");
    EXPECT_EQ(items.Count(), 3);
    EXPECT_EQ(items.GetObject(2).GetInt("id"), 2);
    std::filesystem::remove(path);
}

TEST(JsonFile, LoadFromMissingFileReturnsEmpty) {
    const JsonObject obj = JsonFile::LoadFromFile("does_not_exist.json");
    EXPECT_TRUE(obj.IsEmpty());
}

TEST(JsonFile, SaveToInvalidPathReturnsFalse) {
    JsonObject obj;
    obj.Set("x", 1);
    EXPECT_FALSE(JsonFile::SaveToFile(obj, "/invalid/path/that/cannot/exist/file.json"));
}
