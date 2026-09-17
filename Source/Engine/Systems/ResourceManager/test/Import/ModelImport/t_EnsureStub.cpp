#include <ResourceManager/Import/ModelImport/ModelImport.h>

#include <Utils/Serialization/JsonFile.h>
#include <Utils/Serialization/JsonObject.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

using namespace nous::engine::resource_manager;

namespace
{
    std::string ScratchPath(const std::string& name)
    {
        return (std::filesystem::temp_directory_path() / name).string();
    }

    void WriteStub(const std::string& path, const JsonObject& json)
    {
        ASSERT_TRUE(JsonFile::SaveToFile(json, path));
    }
}

// THE BUG, found in QA 2026-09-13.
//
// A model imported at Assets/Rig.fbx and later moved to Assets/Rigs/Rig.fbx left its
// sibling stubs pointing at the old path forever: EnsureStub returned the moment the
// file existed. Nothing noticed until Library/ was deleted, because `source` is read
// only by the stub importers' fallback re-parse -- and then assimp reported "Unable to
// open file" naming a path the user had not used in weeks.
TEST(t_EnsureStub, RewritesASourceThatNoLongerNamesTheModel)
{
    const std::string stub = ScratchPath("t_EnsureStub_stale.nskel");

    JsonObject original;
    original.Set("source", "Assets/Rig.fbx");
    WriteStub(stub, original);

    EXPECT_TRUE(EnsureStub(stub, "Assets/Rigs/Rig.fbx", ""));

    const JsonObject reloaded = JsonFile::LoadFromFile(stub);
    EXPECT_EQ(reloaded.GetString("source"), "Assets/Rigs/Rig.fbx");

    std::filesystem::remove(stub);
}

// `source` is a DERIVED back-pointer and is the only key reconciled. Everything else
// in the file is the user's: the clip name the fallback re-parse matches against the
// aiScene, and a .nanim's authored loop/speed. Rewriting the whole stub is what the
// never-overwrite rule exists to prevent, and it would silently reset playback
// settings on every re-import of a moved model.
TEST(t_EnsureStub, ReconcilingTheSourceKeepsEveryAuthoredKey)
{
    const std::string stub = ScratchPath("t_EnsureStub_authored.nanim");

    JsonObject original;
    original.Set("source", "Assets/Rig.fbx");
    original.Set("clip",   "mixamo.com");
    original.Set("loop",   false);
    original.Set("speed",  0.25f);
    WriteStub(stub, original);

    EXPECT_TRUE(EnsureStub(stub, "Assets/Rigs/Rig.fbx", "mixamo.com"));

    const JsonObject reloaded = JsonFile::LoadFromFile(stub);
    EXPECT_EQ(reloaded.GetString("source"), "Assets/Rigs/Rig.fbx");
    EXPECT_EQ(reloaded.GetString("clip"),   "mixamo.com");
    EXPECT_FALSE(reloaded.GetBool("loop", true));
    EXPECT_FLOAT_EQ(reloaded.GetFloat("speed", 1.0f), 0.25f);

    std::filesystem::remove(stub);
}

// The common path by a wide margin: nothing moved, so nothing is written. Asserted
// through the file's LAST WRITE TIME rather than its contents -- identical contents
// would also be produced by a rewrite, and a stub rewritten on every scan is a
// needless disk write per asset per launch.
TEST(t_EnsureStub, AStubWhoseSourceAlreadyMatchesIsNotRewritten)
{
    const std::string stub = ScratchPath("t_EnsureStub_match.nskel");

    JsonObject original;
    original.Set("source", "Assets/Rigs/Rig.fbx");
    WriteStub(stub, original);

    const auto before = std::filesystem::last_write_time(stub);

    EXPECT_TRUE(EnsureStub(stub, "Assets/Rigs/Rig.fbx", ""));

    EXPECT_EQ(std::filesystem::last_write_time(stub), before);

    std::filesystem::remove(stub);
}

// A SEPARATOR FLIP IS NOT A MOVE.
//
// Stubs on disk carry Windows backslashes (that is what the engine writes), while a
// scanned assetsPath may use forward slashes. Compared as raw strings, every stub in
// the project would read as stale and be rewritten on every launch -- a needless disk
// write per asset, and a working tree full of spurious diffs.
TEST(t_EnsureStub, APathDifferingOnlyBySeparatorIsNotTreatedAsAMove)
{
    const std::string stub = ScratchPath("t_EnsureStub_seps.nskel");

    JsonObject original;
    original.Set("source", "Assets\\Rigs\\Rig.fbx");
    WriteStub(stub, original);

    const auto before = std::filesystem::last_write_time(stub);

    EXPECT_TRUE(EnsureStub(stub, "Assets/Rigs/Rig.fbx", ""));

    EXPECT_EQ(std::filesystem::last_write_time(stub), before);
    EXPECT_EQ(JsonFile::LoadFromFile(stub).GetString("source"), "Assets\\Rigs\\Rig.fbx");

    std::filesystem::remove(stub);
}

// The original behaviour, unchanged: no stub yet means write one.
TEST(t_EnsureStub, WritesTheStubWhenThereIsNone)
{
    const std::string stub = ScratchPath("t_EnsureStub_fresh.nanim");
    std::filesystem::remove(stub);

    EXPECT_TRUE(EnsureStub(stub, "Assets/Rigs/Rig.fbx", "mixamo.com"));

    const JsonObject written = JsonFile::LoadFromFile(stub);
    EXPECT_EQ(written.GetString("source"), "Assets/Rigs/Rig.fbx");
    EXPECT_EQ(written.GetString("clip"),   "mixamo.com");

    std::filesystem::remove(stub);
}

// A skeleton stub carries no clip name, and an empty one must not be written as a
// key -- ReadAuthoringFromStub and the fallback re-parse both treat an absent key as
// "not a clip stub", which an empty string would not satisfy.
TEST(t_EnsureStub, AnEmptyClipNameIsNotWrittenAsAKey)
{
    const std::string stub = ScratchPath("t_EnsureStub_noclip.nskel");
    std::filesystem::remove(stub);

    EXPECT_TRUE(EnsureStub(stub, "Assets/Rigs/Rig.fbx", ""));

    const JsonObject written = JsonFile::LoadFromFile(stub);
    EXPECT_FALSE(written.HasKey("clip"));

    std::filesystem::remove(stub);
}

// A file that does not parse must be LEFT ALONE, not reconciled.
//
// Reconciling reads the old object and writes it back with one key changed, so a
// failed parse would hand back an empty object and "reconcile" the stub down to a
// lone `source` -- destroying the UID pairing's companion data and any authored
// settings, in the one case where the user most needs the file preserved to see what
// went wrong. Returning true is deliberate: this unit's policy is that a sibling
// problem is logged and skipped rather than costing the caller its mesh.
TEST(t_EnsureStub, AnUnparseableStubIsNotClobbered)
{
    const std::string stub = ScratchPath("t_EnsureStub_corrupt.nanim");
    {
        std::ofstream out(stub, std::ios::binary | std::ios::trunc);
        out << "this is not json {{{";
    }

    EXPECT_TRUE(EnsureStub(stub, "Assets/Rigs/Rig.fbx", "mixamo.com"));

    // Scoped so the handle is CLOSED before the remove below: Windows refuses to
    // delete a file that is still open, and the throw surfaces as a failure of this
    // test rather than of the cleanup.
    std::string contents;
    {
        std::ifstream in(stub, std::ios::binary);
        contents.assign((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
    }

    EXPECT_EQ(contents, "this is not json {{{");

    std::filesystem::remove(stub);
}
