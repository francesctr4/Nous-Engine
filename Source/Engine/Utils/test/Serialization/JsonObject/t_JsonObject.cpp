// Covers JsonObject and JsonArray -- the RAII parson wrapper that every
// Component::Serialize/Deserialize and every importer goes through.
// t_Utils_JsonFile next door covers only the disk bridge (LoadFromFile /
// SaveToFile); the value types themselves had no tests.
//
// Three behaviours here are load-bearing and easy to break by "tidying":
//   * Get* returns the caller's default on a MISSING key AND on a WRONG-TYPE key,
//     which is what lets old scene files load against a changed schema.
//   * GetObject/GetArray return an owning DEEP COPY, not a view. Mutating the
//     result must not touch the parent.
//   * Set(key, const char*) is explicitly overloaded so a string literal does not
//     bind to the bool overload -- standard conversions outrank user-defined ones,
//     so without it every literal would silently serialize as `true`.

#include <gtest/gtest.h>

#include <Utils/Serialization/JsonObject.h>
#include <Utils/Serialization/JsonArray.h>

#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <string>
#include <vector>

namespace
{
    constexpr float kEps = 1e-5f;
}

// ===========================================================================
// JsonObject -- scalars
// ===========================================================================

TEST(t_JsonObject, DefaultConstructedHasNoKeysButIsNotIsEmpty)
{
    // IsEmpty() does NOT mean "has no keys" -- it reports m_Value == nullptr,
    // i.e. "this is the null object handed back for a missing/wrong-type key".
    // The default constructor calls json_value_init_object(), so a freshly
    // built object is a real, writable, key-less object and IsEmpty() is false.
    // Read it as IsNull(). Use GetKeys().empty() for the intuitive question.
    const JsonObject o;

    EXPECT_FALSE(o.IsEmpty());
    EXPECT_TRUE(o.GetKeys().empty());
    EXPECT_FALSE(o.HasKey("anything"));
}

TEST(t_JsonObject, IsEmptyDistinguishesAnAbsentKeyFromAKeylessObject)
{
    // The distinction the name hides, pinned in one place: both look "empty" to
    // a caller, only one of them is writable.
    JsonObject parent;
    parent.Set("present", JsonObject{});

    const JsonObject present = parent.GetObject("present");
    const JsonObject absent  = parent.GetObject("absent");

    EXPECT_FALSE(present.IsEmpty());          // a real, key-less child
    EXPECT_TRUE(present.GetKeys().empty());
    EXPECT_TRUE(absent.IsEmpty());            // no such key
}

TEST(t_JsonObject, ScalarsRoundTrip)
{
    JsonObject o;
    o.Set("i", 42);
    o.Set("f", 1.5f);
    o.Set("d", 2.25);
    o.Set("b", true);
    o.Set("s", std::string_view("hello"));

    EXPECT_EQ(o.GetInt("i"), 42);
    EXPECT_NEAR(o.GetFloat("f"), 1.5f, kEps);
    EXPECT_DOUBLE_EQ(o.GetDouble("d"), 2.25);
    EXPECT_TRUE(o.GetBool("b"));
    EXPECT_EQ(o.GetString("s"), "hello");
    EXPECT_FALSE(o.IsEmpty());
}

TEST(t_JsonObject, StringLiteralDoesNotBindToTheBoolOverload)
{
    // The whole reason Set(key, const char*) exists. Without it this stores the
    // boolean `true` and every serialized name in the engine becomes "true".
    JsonObject o;
    o.Set("name", "MainCamera");

    EXPECT_EQ(o.GetString("name"), "MainCamera");
    EXPECT_NE(o.GetString("name"), "true");
}

TEST(t_JsonObject, SetNullCharPointerStoresEmptyString)
{
    JsonObject o;
    const char* nothing = nullptr;
    o.Set("s", nothing);

    EXPECT_EQ(o.GetString("s"), "");
    EXPECT_TRUE(o.HasKey("s"));
}

TEST(t_JsonObject, BoolFalseRoundTripsAndIsDistinctFromMissing)
{
    JsonObject o;
    o.Set("flag", false);

    EXPECT_FALSE(o.GetBool("flag", true));   // stored value wins over the default
    EXPECT_TRUE(o.HasKey("flag"));
}

TEST(t_JsonObject, SetOverwritesAnExistingKey)
{
    JsonObject o;
    o.Set("v", 1);
    o.Set("v", 2);

    EXPECT_EQ(o.GetInt("v"), 2);
}

TEST(t_JsonObject, SetCanChangeAKeysType)
{
    JsonObject o;
    o.Set("v", 1);
    o.Set("v", "now a string");

    EXPECT_EQ(o.GetString("v"), "now a string");
}

// ---------------------------------------------------------------------------
// Missing / wrong-type reads -- the backward-compatibility contract
// ---------------------------------------------------------------------------

TEST(t_JsonObject, MissingKeyReturnsTheSuppliedDefault)
{
    const JsonObject o;

    EXPECT_EQ(o.GetInt("nope", 7), 7);
    EXPECT_NEAR(o.GetFloat("nope", 3.5f), 3.5f, kEps);
    EXPECT_DOUBLE_EQ(o.GetDouble("nope", 9.5), 9.5);
    EXPECT_TRUE(o.GetBool("nope", true));
    EXPECT_EQ(o.GetString("nope", "fallback"), "fallback");
}

TEST(t_JsonObject, MissingKeyReturnsTheZeroDefaultWhenNoneGiven)
{
    const JsonObject o;

    EXPECT_EQ(o.GetInt("nope"), 0);
    EXPECT_NEAR(o.GetFloat("nope"), 0.f, kEps);
    EXPECT_FALSE(o.GetBool("nope"));
    EXPECT_EQ(o.GetString("nope"), "");
}

TEST(t_JsonObject, WrongTypeReadReturnsTheDefault)
{
    // A scene authored before a field changed type must not poison the load.
    JsonObject o;
    o.Set("s", "not a number");
    o.Set("i", 5);

    EXPECT_EQ(o.GetInt("s", -1), -1);
    EXPECT_EQ(o.GetString("i", "fallback"), "fallback");
}

TEST(t_JsonObject, HasKeyIsTrueOnlyForPresentKeys)
{
    JsonObject o;
    o.Set("present", 1);

    EXPECT_TRUE(o.HasKey("present"));
    EXPECT_FALSE(o.HasKey("absent"));
}

TEST(t_JsonObject, RemoveDeletesTheKey)
{
    JsonObject o;
    o.Set("a", 1);
    o.Set("b", 2);

    o.Remove("a");

    EXPECT_FALSE(o.HasKey("a"));
    EXPECT_TRUE(o.HasKey("b"));
    EXPECT_EQ(o.GetInt("b"), 2);
}

TEST(t_JsonObject, RemoveOfAMissingKeyIsANoOp)
{
    JsonObject o;
    o.Set("a", 1);

    EXPECT_NO_FATAL_FAILURE(o.Remove("ghost"));
    EXPECT_TRUE(o.HasKey("a"));
}

TEST(t_JsonObject, GetKeysReturnsEveryKey)
{
    JsonObject o;
    o.Set("alpha", 1);
    o.Set("beta", 2);
    o.Set("gamma", 3);

    auto keys = o.GetKeys();
    std::sort(keys.begin(), keys.end());

    ASSERT_EQ(keys.size(), 3u);
    EXPECT_EQ(keys[0], "alpha");
    EXPECT_EQ(keys[1], "beta");
    EXPECT_EQ(keys[2], "gamma");
}

// ---------------------------------------------------------------------------
// GLM types
// ---------------------------------------------------------------------------

TEST(t_JsonObject, VectorTypesRoundTrip)
{
    JsonObject o;
    o.Set("v2", glm::vec2(1.f, 2.f));
    o.Set("v3", glm::vec3(1.f, 2.f, 3.f));
    o.Set("v4", glm::vec4(1.f, 2.f, 3.f, 4.f));

    const auto v2 = o.GetVec2("v2");
    const auto v3 = o.GetVec3("v3");
    const auto v4 = o.GetVec4("v4");

    EXPECT_NEAR(v2.x, 1.f, kEps); EXPECT_NEAR(v2.y, 2.f, kEps);
    EXPECT_NEAR(v3.x, 1.f, kEps); EXPECT_NEAR(v3.z, 3.f, kEps);
    EXPECT_NEAR(v4.x, 1.f, kEps); EXPECT_NEAR(v4.w, 4.f, kEps);
}

TEST(t_JsonObject, QuaternionRoundTripsInWXYZOrder)
{
    // CTransform serializes orientation as [w, x, y, z]. If the component order
    // is ever flipped, every saved rotation in every scene silently changes.
    JsonObject o;
    o.Set("q", glm::quat(0.5f, 0.1f, 0.2f, 0.3f));   // glm::quat(w, x, y, z)

    const glm::quat q = o.GetQuat("q");

    EXPECT_NEAR(q.w, 0.5f, kEps);
    EXPECT_NEAR(q.x, 0.1f, kEps);
    EXPECT_NEAR(q.y, 0.2f, kEps);
    EXPECT_NEAR(q.z, 0.3f, kEps);
}

TEST(t_JsonObject, MissingVectorReturnsTheSuppliedDefault)
{
    const JsonObject o;

    const auto v3 = o.GetVec3("nope", glm::vec3(9.f, 8.f, 7.f));
    EXPECT_NEAR(v3.x, 9.f, kEps);
    EXPECT_NEAR(v3.y, 8.f, kEps);
    EXPECT_NEAR(v3.z, 7.f, kEps);
}

TEST(t_JsonObject, WrongTypeVectorReadReturnsTheDefault)
{
    JsonObject o;
    o.Set("v", 5);   // a number where an array is expected

    const auto v3 = o.GetVec3("v", glm::vec3(1.f, 1.f, 1.f));
    EXPECT_NEAR(v3.x, 1.f, kEps);
}

TEST(t_JsonObject, NegativeAndFractionalComponentsSurvive)
{
    JsonObject o;
    o.Set("v", glm::vec3(-1.25f, 0.f, 1e3f));

    const auto v = o.GetVec3("v");
    EXPECT_NEAR(v.x, -1.25f, kEps);
    EXPECT_NEAR(v.y, 0.f, kEps);
    EXPECT_NEAR(v.z, 1e3f, 1e-2f);
}

// ---------------------------------------------------------------------------
// Nested objects -- the deep-copy contract
// ---------------------------------------------------------------------------

TEST(t_JsonObject, NestedObjectRoundTrips)
{
    JsonObject child;
    child.Set("inner", 5);

    JsonObject parent;
    parent.Set("child", std::move(child));

    const JsonObject read = parent.GetObject("child");
    EXPECT_EQ(read.GetInt("inner"), 5);
}

TEST(t_JsonObject, GetObjectReturnsADeepCopyNotAView)
{
    // Components read a child, mutate it, and Set it back. If GetObject returned
    // a view, the mutation would land in the parent before the Set and a failed
    // deserialize would leave the parent half-modified.
    JsonObject child;
    child.Set("value", 1);

    JsonObject parent;
    parent.Set("child", std::move(child));

    JsonObject copy = parent.GetObject("child");
    copy.Set("value", 999);

    EXPECT_EQ(parent.GetObject("child").GetInt("value"), 1);
    EXPECT_EQ(copy.GetInt("value"), 999);
}

TEST(t_JsonObject, GetObjectOnAMissingKeyReturnsAnEmptyObject)
{
    const JsonObject o;

    const JsonObject missing = o.GetObject("nope");
    EXPECT_TRUE(missing.IsEmpty());
}

TEST(t_JsonObject, GetObjectOnAScalarKeyReturnsAnEmptyObject)
{
    JsonObject o;
    o.Set("scalar", 1);

    const JsonObject wrong = o.GetObject("scalar");
    EXPECT_TRUE(wrong.IsEmpty());
}

TEST(t_JsonObject, DeeplyNestedObjectsRoundTrip)
{
    JsonObject inner;
    inner.Set("depth", 3);

    JsonObject middle;
    middle.Set("inner", std::move(inner));

    JsonObject outer;
    outer.Set("middle", std::move(middle));

    EXPECT_EQ(outer.GetObject("middle").GetObject("inner").GetInt("depth"), 3);
}

// ---------------------------------------------------------------------------
// Move semantics -- the type is move-only and owns its parson value
// ---------------------------------------------------------------------------

TEST(t_JsonObject, MoveConstructionTransfersContents)
{
    JsonObject src;
    src.Set("v", 7);

    const JsonObject dst(std::move(src));

    EXPECT_EQ(dst.GetInt("v"), 7);
}

TEST(t_JsonObject, MoveAssignmentTransfersContents)
{
    JsonObject src;
    src.Set("v", 7);

    JsonObject dst;
    dst.Set("other", 1);
    dst = std::move(src);

    EXPECT_EQ(dst.GetInt("v"), 7);
    EXPECT_FALSE(dst.HasKey("other"));   // the old contents are released
}

// ===========================================================================
// JsonArray
// ===========================================================================

TEST(t_JsonArray, DefaultConstructedHasNoElementsButIsNotIsEmpty)
{
    // Same trap as JsonObject::IsEmpty -- it reports "null array", not "no
    // elements". Count() == 0 is the question callers usually mean.
    const JsonArray a;

    EXPECT_FALSE(a.IsEmpty());
    EXPECT_EQ(a.Count(), 0);
}

TEST(t_JsonArray, AppendedScalarsReadBackInOrder)
{
    JsonArray a;
    a.Append(10);
    a.Append(20);
    a.Append(30);

    ASSERT_EQ(a.Count(), 3);
    EXPECT_EQ(a.GetInt(0), 10);
    EXPECT_EQ(a.GetInt(1), 20);
    EXPECT_EQ(a.GetInt(2), 30);
    EXPECT_FALSE(a.IsEmpty());
}

TEST(t_JsonArray, MixedScalarTypesRoundTrip)
{
    JsonArray a;
    a.Append(1);
    a.Append(2.5f);
    a.Append(3.25);
    a.Append(std::string_view("four"));

    EXPECT_EQ(a.GetInt(0), 1);
    EXPECT_NEAR(a.GetFloat(1), 2.5f, kEps);
    EXPECT_DOUBLE_EQ(a.GetDouble(2), 3.25);
    EXPECT_EQ(a.GetString(3), "four");
}

TEST(t_JsonArray, OutOfRangeReadsReturnDefaults)
{
    JsonArray a;
    a.Append(1);

    EXPECT_EQ(a.GetInt(5), 0);
    EXPECT_EQ(a.GetString(5), "");
    EXPECT_TRUE(a.GetObject(5).IsEmpty());
    EXPECT_TRUE(a.GetArray(5).IsEmpty());
}

TEST(t_JsonArray, NegativeIndexReadsReturnDefaults)
{
    JsonArray a;
    a.Append(1);

    EXPECT_EQ(a.GetInt(-1), 0);
    EXPECT_EQ(a.GetString(-1), "");
}

TEST(t_JsonArray, WrongTypeReadReturnsTheDefault)
{
    JsonArray a;
    a.Append(std::string_view("text"));

    EXPECT_EQ(a.GetInt(0), 0);
}

TEST(t_JsonArray, RemoveShiftsSubsequentElementsDown)
{
    JsonArray a;
    a.Append(10);
    a.Append(20);
    a.Append(30);

    a.Remove(1);

    ASSERT_EQ(a.Count(), 2);
    EXPECT_EQ(a.GetInt(0), 10);
    EXPECT_EQ(a.GetInt(1), 30);
}

TEST(t_JsonArray, RemoveOutOfRangeIsANoOp)
{
    JsonArray a;
    a.Append(10);

    EXPECT_NO_FATAL_FAILURE(a.Remove(9));
    EXPECT_EQ(a.Count(), 1);
}

TEST(t_JsonArray, AppendedObjectsRoundTrip)
{
    JsonObject first;
    first.Set("id", 1);
    JsonObject second;
    second.Set("id", 2);

    JsonArray a;
    a.Append(std::move(first));
    a.Append(std::move(second));

    ASSERT_EQ(a.Count(), 2);
    EXPECT_EQ(a.GetObject(0).GetInt("id"), 1);
    EXPECT_EQ(a.GetObject(1).GetInt("id"), 2);
}

TEST(t_JsonArray, GetObjectReturnsADeepCopyNotAView)
{
    JsonObject child;
    child.Set("id", 1);

    JsonArray a;
    a.Append(std::move(child));

    JsonObject copy = a.GetObject(0);
    copy.Set("id", 999);

    EXPECT_EQ(a.GetObject(0).GetInt("id"), 1);
}

TEST(t_JsonArray, NestedArraysRoundTrip)
{
    JsonArray inner;
    inner.Append(1);
    inner.Append(2);

    JsonArray outer;
    outer.Append(std::move(inner));

    ASSERT_EQ(outer.Count(), 1);
    const JsonArray read = outer.GetArray(0);
    ASSERT_EQ(read.Count(), 2);
    EXPECT_EQ(read.GetInt(0), 1);
    EXPECT_EQ(read.GetInt(1), 2);
}

TEST(t_JsonArray, MoveAssignmentTransfersContents)
{
    JsonArray src;
    src.Append(1);

    JsonArray dst;
    dst.Append(99);
    dst.Append(98);
    dst = std::move(src);

    ASSERT_EQ(dst.Count(), 1);
    EXPECT_EQ(dst.GetInt(0), 1);
}

// ===========================================================================
// Object <-> Array interop
// ===========================================================================

TEST(t_JsonObject, ArrayStoredOnAnObjectRoundTrips)
{
    JsonArray a;
    a.Append(1);
    a.Append(2);

    JsonObject o;
    o.Set("values", std::move(a));

    const JsonArray read = o.GetArray("values");
    ASSERT_EQ(read.Count(), 2);
    EXPECT_EQ(read.GetInt(0), 1);
    EXPECT_EQ(read.GetInt(1), 2);
}

TEST(t_JsonObject, GetArrayOnAMissingKeyReturnsAnEmptyArray)
{
    const JsonObject o;

    const JsonArray missing = o.GetArray("nope");
    EXPECT_TRUE(missing.IsEmpty());   // null, not merely element-less
    EXPECT_EQ(missing.Count(), 0);
}

TEST(t_JsonObject, GetArrayOnAScalarKeyReturnsAnEmptyArray)
{
    JsonObject o;
    o.Set("scalar", 1);

    EXPECT_TRUE(o.GetArray("scalar").IsEmpty());
}

TEST(t_JsonObject, ArrayOfObjectsOnAnObjectRoundTrips)
{
    // The shape every component list serializes to: { "components": [ {...}, {...} ] }
    JsonObject a;
    a.Set("type", "CTransform");
    JsonObject b;
    b.Set("type", "CMesh");

    JsonArray list;
    list.Append(std::move(a));
    list.Append(std::move(b));

    JsonObject root;
    root.Set("components", std::move(list));

    const JsonArray read = root.GetArray("components");
    ASSERT_EQ(read.Count(), 2);
    EXPECT_EQ(read.GetObject(0).GetString("type"), "CTransform");
    EXPECT_EQ(read.GetObject(1).GetString("type"), "CMesh");
}
