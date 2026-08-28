#include <gtest/gtest.h>
#include <EngineCore/InvalidID.h>

#include <ResourceManager/Types/ResourceShader/ResourceShader.h>
#include <ShaderSystem/ShaderTypes.h>

// =============================================================================
// Constructor
// =============================================================================

TEST(t_ResourceShader, ConstructorSetsTypeShader)
{
    ResourceShader s(1);
    EXPECT_EQ(s.GetType(), ResourceType::SHADER);
}

TEST(t_ResourceShader, ConstructorSetsUID)
{
    ResourceShader s(42);
    EXPECT_EQ(s.GetUID(), 42u);
}

TEST(t_ResourceShader, DefaultConstructorUID_IsZero)
{
    ResourceShader s;
    EXPECT_EQ(s.GetUID(), 0u);
}

TEST(t_ResourceShader, GenerationStartsAtINVALID_ID)
{
    ResourceShader s(1);
    EXPECT_EQ(s.generation, INVALID_ID);
}

TEST(t_ResourceShader, DefaultStagesData_IsEmpty)
{
    ResourceShader s(1);
    EXPECT_TRUE(s.stagesData.empty());
}

TEST(t_ResourceShader, DefaultInternalData_IsNull)
{
    ResourceShader s(1);
    EXPECT_EQ(s.internalData, nullptr);
}

// =============================================================================
// Inherited Resource interface
// =============================================================================

TEST(t_ResourceShader, RefCountStartsAtZero)
{
    ResourceShader s(1);
    EXPECT_EQ(s.GetReferenceCount(), 0u);
}

TEST(t_ResourceShader, IncreaseReferenceCount_Increments)
{
    ResourceShader s(1);
    s.IncreaseReferenceCount();
    EXPECT_EQ(s.GetReferenceCount(), 1u);
}

TEST(t_ResourceShader, DecreaseReferenceCount_Decrements)
{
    ResourceShader s(1);
    s.IncreaseReferenceCount();
    s.DecreaseReferenceCount();
    EXPECT_EQ(s.GetReferenceCount(), 0u);
}

TEST(t_ResourceShader, SetName_GetName_Roundtrip)
{
    ResourceShader s;
    s.SetName("MyShader");
    EXPECT_EQ(s.GetName(), "MyShader");
}

TEST(t_ResourceShader, SetAssetsPath_GetAssetsPath_Roundtrip)
{
    ResourceShader s;
    s.SetAssetsPath("Assets/Shaders/Test.glsl");
    EXPECT_EQ(s.GetAssetsPath(), "Assets/Shaders/Test.glsl");
}

TEST(t_ResourceShader, SetLibraryPath_GetLibraryPath_Roundtrip)
{
    ResourceShader s;
    s.SetLibraryPath("Library/Shaders/42");
    EXPECT_EQ(s.GetLibraryPath(), "Library/Shaders/42");
}

// =============================================================================
// stagesData mutation
// =============================================================================

TEST(t_ResourceShader, StagesData_CanBePushed)
{
    ResourceShader s(1);
    ShaderSource src;
    src.stage = ShaderStage::Vertex;
    s.stagesData.push_back(src);
    ASSERT_EQ(s.stagesData.size(), 1u);
    EXPECT_EQ(s.stagesData[0].stage, ShaderStage::Vertex);
}

TEST(t_ResourceShader, GenerationCanBeIncremented)
{
    ResourceShader s(1);
    s.generation = 0;
    ++s.generation;
    EXPECT_EQ(s.generation, 1u);
}
