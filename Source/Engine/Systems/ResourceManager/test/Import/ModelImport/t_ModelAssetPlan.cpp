#include <ResourceManager/Import/ModelImport/ModelAssetPlan.h>

#include <gtest/gtest.h>

using namespace nous::engine::resource_manager;
using nous::engine::animation_system::AnimChannel;
using nous::engine::animation_system::AnimClipData;

namespace
{
    AnimClipData Clip(std::string name, size_t channelCount)
    {
        AnimClipData clip;
        clip.name     = std::move(name);
        clip.duration = 2.0f;
        clip.channels.resize(channelCount);

        for (size_t i = 0; i < channelCount; ++i)
            clip.channels[i].boneName = "bone" + std::to_string(i);

        return clip;
    }

    // HasSkeleton() is BoneCount() > 0, so the arrays only need to be the right
    // length and mutually consistent.
    ModelImportData ModelWithSkeleton(size_t boneCount)
    {
        ModelImportData model;
        model.skeleton.names.resize(boneCount, "bone");
        model.skeleton.parents.resize(boneCount, -1);
        model.skeleton.offsets.resize(boneCount, glm::mat4(1.0f));
        model.skeleton.bindLocals.resize(boneCount);
        return model;
    }
}

TEST(t_ModelAssetPlan, SkeletonStubIsASiblingOfTheModel)
{
    const auto plan = PlanModelAssets(ModelWithSkeleton(65),
                                      "Assets/RumbaDancing_WithSkin.fbx");

    EXPECT_EQ(plan.skeletonStubPath, "Assets/RumbaDancing_WithSkin.nskel");
}

TEST(t_ModelAssetPlan, NoSkeletonStubWhenTheModelHasNoBones)
{
    const ModelImportData model;   // a static prop: no skeleton, no clips

    const auto plan = PlanModelAssets(model, "Assets/Rock.fbx");

    EXPECT_TRUE(plan.skeletonStubPath.empty());
    EXPECT_TRUE(plan.clips.empty());
}

TEST(t_ModelAssetPlan, SanitizeReplacesCharactersThatAreAwkwardInFilenames)
{
    // The one that matters in practice: Mixamo names its clip "mixamo.com", so an
    // unsanitized stub would be "Model@mixamo.com.nanim" -- an asset whose
    // extension parses as ".com".
    EXPECT_EQ(SanitizeClipFileName("mixamo.com"),   "mixamo_com");
    EXPECT_EQ(SanitizeClipFileName("Walk Forward"), "Walk_Forward");
    EXPECT_EQ(SanitizeClipFileName("a/b\\c:d*e?f\"g<h>i|j"), "a_b_c_d_e_f_g_h_i_j");
    EXPECT_EQ(SanitizeClipFileName(""),             "Animation");
}

TEST(t_ModelAssetPlan, ClipStubUsesTheModelStemAndAnAtSeparator)
{
    ModelImportData model = ModelWithSkeleton(2);
    model.clips.push_back(Clip("mixamo.com", 53));

    const auto plan = PlanModelAssets(model, "Assets/RumbaDancing_WithSkin.fbx");

    ASSERT_EQ(plan.clips.size(), 1u);
    EXPECT_EQ(plan.clips[0].stubPath, "Assets/RumbaDancing_WithSkin@mixamo_com.nanim");
    EXPECT_EQ(plan.clips[0].clipName, "mixamo.com");   // ORIGINAL, for the re-parse
    EXPECT_EQ(plan.clips[0].clipIndex, 0u);
}

// RumbaDancing_WithSkin.fbx really does carry a second AnimationStack, "Take 001",
// bound to an empty layer: a real 3.33s duration and ZERO curve nodes. Assimp
// reports it. Without this it becomes a phantom animation resource.
TEST(t_ModelAssetPlan, ClipsWithNoChannelsAreDropped)
{
    ModelImportData model = ModelWithSkeleton(2);
    model.clips.push_back(Clip("Take 001",   0));
    model.clips.push_back(Clip("mixamo.com", 53));

    const auto plan = PlanModelAssets(model, "Assets/RumbaDancing_WithSkin.fbx");

    ASSERT_EQ(plan.clips.size(), 1u);
    EXPECT_EQ(plan.clips[0].clipName, "mixamo.com");
}

// clipIndex indexes the ORIGINAL clips array. If dropping shifted it, the executor
// would write clip N's binary out of clip N+1's data -- silent, and very confusing
// to debug from the symptom.
TEST(t_ModelAssetPlan, ClipIndexStillPointsAtTheOriginalClipAfterDrops)
{
    ModelImportData model = ModelWithSkeleton(2);
    model.clips.push_back(Clip("Empty", 0));
    model.clips.push_back(Clip("Walk",  4));

    const auto plan = PlanModelAssets(model, "Assets/Char.fbx");

    ASSERT_EQ(plan.clips.size(), 1u);
    EXPECT_EQ(plan.clips[0].clipIndex, 1u);
    EXPECT_EQ(model.clips[plan.clips[0].clipIndex].name, "Walk");
}

TEST(t_ModelAssetPlan, DuplicateClipNamesAreDisambiguated)
{
    ModelImportData model = ModelWithSkeleton(2);
    model.clips.push_back(Clip("Walk", 4));
    model.clips.push_back(Clip("Walk", 4));
    model.clips.push_back(Clip("Walk", 4));

    const auto plan = PlanModelAssets(model, "Assets/Char.fbx");

    ASSERT_EQ(plan.clips.size(), 3u);
    EXPECT_EQ(plan.clips[0].stubPath, "Assets/Char@Walk.nanim");
    EXPECT_EQ(plan.clips[1].stubPath, "Assets/Char@Walk_1.nanim");
    EXPECT_EQ(plan.clips[2].stubPath, "Assets/Char@Walk_2.nanim");
}

// Uniqueness is enforced on the SANITIZED name: two distinct clip names can
// collapse onto one filename, and the collision is invisible in the originals.
TEST(t_ModelAssetPlan, DisambiguationHappensAfterSanitizing)
{
    ModelImportData model = ModelWithSkeleton(2);
    model.clips.push_back(Clip("a.b", 4));
    model.clips.push_back(Clip("a b", 4));

    const auto plan = PlanModelAssets(model, "Assets/Char.fbx");

    ASSERT_EQ(plan.clips.size(), 2u);
    EXPECT_EQ(plan.clips[0].stubPath, "Assets/Char@a_b.nanim");
    EXPECT_EQ(plan.clips[1].stubPath, "Assets/Char@a_b_1.nanim");
}

// RumbaDancing_WithoutSkin.fbx: 65 bones and a take, no geometry at all.
TEST(t_ModelAssetPlan, MeshlessModelStillPlansItsSkeletonAndClips)
{
    ModelImportData model = ModelWithSkeleton(65);
    model.clips.push_back(Clip("mixamo.com", 53));
    ASSERT_TRUE(model.submeshes.empty());

    const auto plan = PlanModelAssets(model, "Assets/RumbaDancing_WithoutSkin.fbx");

    EXPECT_EQ(plan.skeletonStubPath, "Assets/RumbaDancing_WithoutSkin.nskel");
    ASSERT_EQ(plan.clips.size(), 1u);
    EXPECT_EQ(plan.clips[0].stubPath, "Assets/RumbaDancing_WithoutSkin@mixamo_com.nanim");
}

TEST(t_ModelAssetPlan, StubsStayInTheModelsOwnDirectory)
{
    ModelImportData model = ModelWithSkeleton(2);
    model.clips.push_back(Clip("Idle", 4));

    const auto plan = PlanModelAssets(model, "Assets/Characters/Hero/Hero.fbx");

    EXPECT_EQ(plan.skeletonStubPath, "Assets/Characters/Hero/Hero.nskel");
    EXPECT_EQ(plan.clips[0].stubPath, "Assets/Characters/Hero/Hero@Idle.nanim");
}

// A dot in a DIRECTORY name must not be mistaken for the model's extension.
TEST(t_ModelAssetPlan, ADotInADirectoryNameIsNotTheExtension)
{
    const ModelImportData model = ModelWithSkeleton(2);

    const auto plan = PlanModelAssets(model, "Assets/v1.0/Hero.fbx");

    EXPECT_EQ(plan.skeletonStubPath, "Assets/v1.0/Hero.nskel");
}
