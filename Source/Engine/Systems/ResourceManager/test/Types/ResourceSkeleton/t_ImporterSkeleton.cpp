#include <gtest/gtest.h>

#include <ResourceManager/Core/MetaFileData.h>
#include <ResourceManager/Types/ResourceSkeleton/ImporterSkeleton.h>
#include <ResourceManager/Types/ResourceSkeleton/ResourceSkeleton.h>

#include <filesystem>
#include <fstream>
#include <string>

using nous::engine::animation_system::SkeletonData;
using nous::engine::animation_system::Transform;
using nous::engine::resource_manager::HashBoneNames;

namespace
{
    // Scratch files go under the OS temp dir, not the working directory: CI runs
    // ctest --parallel and two suites writing a same-named file into bin/ would
    // race. Same rule as t_ResourceAudioGraph.
    std::string ScratchPath(const std::string& name)
    {
        return (std::filesystem::temp_directory_path() / name).string();
    }

    // Root -> Spine -> Head, topologically ordered. Distinct offsets and bind
    // locals per bone so a transposed or off-by-one read cannot pass by accident.
    SkeletonData Rig()
    {
        SkeletonData s;
        s.names   = { "Root", "Spine", "Head" };
        s.parents = { -1, 0, 1 };
        s.offsets = { glm::mat4(1.0f), glm::mat4(2.0f), glm::mat4(3.0f) };

        s.bindLocals.resize(3);
        s.bindLocals[1].position = glm::vec3(0.0f, 1.0f, 0.0f);
        s.bindLocals[2].position = glm::vec3(0.0f, 2.0f, 0.0f);
        s.bindLocals[2].rotation = glm::quat(0.7071f, 0.7071f, 0.0f, 0.0f);
        s.bindLocals[2].scale    = glm::vec3(0.5f, 0.25f, 2.0f);

        s.RebuildLookup();
        return s;
    }

    MetaFileData MetaFor(const std::string& fileName)
    {
        MetaFileData meta;
        meta.uid          = 4242;
        meta.name         = "TestRig";
        meta.resourceType = ResourceType::SKELETON;
        meta.assetsPath   = "Assets/TestRig.nskel";
        meta.libraryPath  = ScratchPath(fileName);
        return meta;
    }
}

TEST(t_ImporterSkeleton, RoundTripsEveryFieldThroughTheRealWriter)
{
    const MetaFileData meta   = MetaFor("t_ImporterSkeleton_roundtrip.nskel");
    const SkeletonData source = Rig();

    ASSERT_TRUE(ImporterSkeleton::SaveSkeleton(meta, source));

    ResourceSkeleton loaded(meta.uid);
    ImporterSkeleton importer;
    ASSERT_TRUE(importer.Deserialize(meta.libraryPath, &loaded));

    ASSERT_EQ(loaded.skeleton.BoneCount(), 3u);
    EXPECT_EQ(loaded.skeleton.names,   source.names);
    EXPECT_EQ(loaded.skeleton.parents, source.parents);

    for (size_t b = 0; b < 3; ++b)
    {
        for (int c = 0; c < 4; ++c)
        {
            for (int r = 0; r < 4; ++r)
                EXPECT_FLOAT_EQ(loaded.skeleton.offsets[b][c][r], source.offsets[b][c][r]);
        }

        EXPECT_EQ(loaded.skeleton.bindLocals[b].position, source.bindLocals[b].position);
        EXPECT_EQ(loaded.skeleton.bindLocals[b].rotation, source.bindLocals[b].rotation);
        EXPECT_EQ(loaded.skeleton.bindLocals[b].scale,    source.bindLocals[b].scale);
    }

    std::filesystem::remove(meta.libraryPath);
}

// lookup is deliberately not serialized -- a string map in a binary duplicates the
// names array. Deserialize must rebuild it, or every FindBone returns -1 and the
// symptom looks like a broken rig rather than a broken load.
TEST(t_ImporterSkeleton, RebuildsTheNameLookupOnLoad)
{
    const MetaFileData meta = MetaFor("t_ImporterSkeleton_lookup.nskel");
    ASSERT_TRUE(ImporterSkeleton::SaveSkeleton(meta, Rig()));

    ResourceSkeleton loaded(meta.uid);
    ImporterSkeleton importer;
    ASSERT_TRUE(importer.Deserialize(meta.libraryPath, &loaded));

    EXPECT_EQ(loaded.skeleton.FindBone("Root"),  0);
    EXPECT_EQ(loaded.skeleton.FindBone("Spine"), 1);
    EXPECT_EQ(loaded.skeleton.FindBone("Head"),  2);
    EXPECT_EQ(loaded.skeleton.FindBone("Absent"), -1);

    std::filesystem::remove(meta.libraryPath);
}

TEST(t_ImporterSkeleton, PreservesTheStructuralInvariants)
{
    const MetaFileData meta = MetaFor("t_ImporterSkeleton_invariants.nskel");
    ASSERT_TRUE(ImporterSkeleton::SaveSkeleton(meta, Rig()));

    ResourceSkeleton loaded(meta.uid);
    ImporterSkeleton importer;
    ASSERT_TRUE(importer.Deserialize(meta.libraryPath, &loaded));

    EXPECT_TRUE(loaded.skeleton.IsConsistent());
    EXPECT_TRUE(loaded.skeleton.IsTopologicallySorted());

    std::filesystem::remove(meta.libraryPath);
}

TEST(t_ImporterSkeleton, StoresAHashOfTheBoneNames)
{
    const MetaFileData meta = MetaFor("t_ImporterSkeleton_hash.nskel");
    ASSERT_TRUE(ImporterSkeleton::SaveSkeleton(meta, Rig()));

    ResourceSkeleton loaded(meta.uid);
    ImporterSkeleton importer;
    ASSERT_TRUE(importer.Deserialize(meta.libraryPath, &loaded));

    EXPECT_NE(loaded.nameHash, 0u);
    EXPECT_EQ(loaded.nameHash, HashBoneNames(Rig().names));

    std::filesystem::remove(meta.libraryPath);
}

// The terminator after each name is what stops these two hashing alike.
TEST(t_ImporterSkeleton, NameHashDistinguishesDifferentSplitsOfTheSameLetters)
{
    EXPECT_NE(HashBoneNames({ "ab", "c" }), HashBoneNames({ "a", "bc" }));
}

TEST(t_ImporterSkeleton, RejectsAForeignMagicRatherThanParsingIt)
{
    const MetaFileData meta = MetaFor("t_ImporterSkeleton_badmagic.nskel");
    {
        std::ofstream out(meta.libraryPath, std::ios::binary);
        const uint32_t wrongMagic = 0xFA7C0DE3u;   // the MESH magic
        out.write(reinterpret_cast<const char*>(&wrongMagic), sizeof(wrongMagic));
    }

    ResourceSkeleton loaded(meta.uid);
    ImporterSkeleton importer;
    EXPECT_FALSE(importer.Deserialize(meta.libraryPath, &loaded));
    EXPECT_EQ(loaded.skeleton.BoneCount(), 0u);

    std::filesystem::remove(meta.libraryPath);
}

TEST(t_ImporterSkeleton, RejectsATruncatedFile)
{
    const MetaFileData meta = MetaFor("t_ImporterSkeleton_truncated.nskel");
    ASSERT_TRUE(ImporterSkeleton::SaveSkeleton(meta, Rig()));

    const auto full = std::filesystem::file_size(meta.libraryPath);
    std::filesystem::resize_file(meta.libraryPath, full / 2);

    ResourceSkeleton loaded(meta.uid);
    ImporterSkeleton importer;
    EXPECT_FALSE(importer.Deserialize(meta.libraryPath, &loaded));

    std::filesystem::remove(meta.libraryPath);
}

TEST(t_ImporterSkeleton, WritesAnEmptySkeletonWithoutComplaining)
{
    const MetaFileData meta = MetaFor("t_ImporterSkeleton_empty.nskel");
    ASSERT_TRUE(ImporterSkeleton::SaveSkeleton(meta, SkeletonData{}));

    ResourceSkeleton loaded(meta.uid);
    ImporterSkeleton importer;
    ASSERT_TRUE(importer.Deserialize(meta.libraryPath, &loaded));
    EXPECT_EQ(loaded.skeleton.BoneCount(), 0u);

    std::filesystem::remove(meta.libraryPath);
}

// A skeleton is never a GPU object -- the bone palette is per-animator and rebuilt
// each frame -- so these three must tolerate a null factory.
TEST(t_ImporterSkeleton, HasNoGpuResidency)
{
    ResourceSkeleton resource(1);
    resource.skeleton = Rig();

    ImporterSkeleton importer;
    EXPECT_TRUE(importer.Upload(&resource, nullptr));
    importer.Release(&resource, nullptr);

    importer.Evict(&resource);
    EXPECT_EQ(resource.skeleton.BoneCount(), 0u);
}
