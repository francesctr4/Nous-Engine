#include <gtest/gtest.h>

#include "Engine/Systems/ShaderSystem/ShaderParser/include/ShaderParser.h"

using namespace nous::engine::shader_system;

// =====================================================
// ShaderParser Unit Tests
// =====================================================

class t_ShaderParser : public ::testing::Test {
protected:
    // Reusable minimal GLSL bodies (no #pragma stage header)
    const std::string kVertBody =
        "#version 450\n"
        "void main() { gl_Position = vec4(0.0); }\n";

    const std::string kFragBody =
        "#version 450\n"
        "layout(location = 0) out vec4 outColor;\n"
        "void main() { outColor = vec4(1.0); }\n";
};

// =====================================================
// Error Cases
// =====================================================

TEST_F(t_ShaderParser, EmptySource_ReturnFailure)
{
    ParseResult result = ParseShaderStages("");
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());
    EXPECT_TRUE(result.stages.empty());
}

TEST_F(t_ShaderParser, NoPragmaDirectives_ReturnFailure)
{
    const std::string source =
        "#version 450\n"
        "void main() {}\n";
    ParseResult result = ParseShaderStages(source);
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());
    EXPECT_TRUE(result.stages.empty());
}

TEST_F(t_ShaderParser, UnknownStageName_ReturnFailure)
{
    const std::string source = "#pragma stage badstage\nvoid main() {}\n";
    ParseResult result = ParseShaderStages(source);
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());
}

TEST_F(t_ShaderParser, UnknownStageName_ErrorMessageContainsStageName)
{
    const std::string source = "#pragma stage myCustomStage\nvoid main() {}\n";
    ParseResult result = ParseShaderStages(source);
    EXPECT_NE(result.errorMessage.find("myCustomStage"), std::string::npos);
}

// =====================================================
// Single Stage - Happy Path
// =====================================================

TEST_F(t_ShaderParser, SingleVertexStage_ReturnSuccess)
{
    const std::string source = "#pragma stage vertex\n" + kVertBody;
    ParseResult result = ParseShaderStages(source);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.errorMessage.empty());
}

TEST_F(t_ShaderParser, SingleVertexStage_ReturnOneStage)
{
    const std::string source = "#pragma stage vertex\n" + kVertBody;
    ParseResult result = ParseShaderStages(source);
    ASSERT_EQ(result.stages.size(), 1u);
    EXPECT_EQ(result.stages[0].stage, ShaderStage::Vertex);
}

TEST_F(t_ShaderParser, SingleFragmentStage_ReturnSuccess)
{
    const std::string source = "#pragma stage fragment\n" + kFragBody;
    ParseResult result = ParseShaderStages(source);
    EXPECT_TRUE(result.success);
    ASSERT_EQ(result.stages.size(), 1u);
    EXPECT_EQ(result.stages[0].stage, ShaderStage::Fragment);
}

TEST_F(t_ShaderParser, SingleStage_SourceContentIsExtracted)
{
    const std::string source = "#pragma stage fragment\nvoid frag() {}\n";
    ParseResult result = ParseShaderStages(source);
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.stages.size(), 1u);
    EXPECT_FALSE(result.stages[0].glslSource.empty());
    EXPECT_NE(result.stages[0].glslSource.find("void frag()"), std::string::npos);
}

TEST_F(t_ShaderParser, SingleStage_SourceDoesNotContainPragmaLine)
{
    const std::string source = "#pragma stage vertex\nvoid main() {}\n";
    ParseResult result = ParseShaderStages(source);
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.stages.size(), 1u);
    EXPECT_EQ(result.stages[0].glslSource.find("#pragma stage"), std::string::npos);
}

TEST_F(t_ShaderParser, SingleStageWithNothingAfterPragma_ReturnSuccess)
{
    // Just the pragma directive, no stage body -- should still succeed
    const std::string source = "#pragma stage vertex\n";
    ParseResult result = ParseShaderStages(source);
    EXPECT_TRUE(result.success);
    ASSERT_EQ(result.stages.size(), 1u);
    EXPECT_EQ(result.stages[0].stage, ShaderStage::Vertex);
}

// =====================================================
// Multiple Stages
// =====================================================

TEST_F(t_ShaderParser, VertexAndFragment_ReturnTwoStages)
{
    const std::string source =
        "#pragma stage vertex\n" + kVertBody +
        "#pragma stage fragment\n" + kFragBody;
    ParseResult result = ParseShaderStages(source);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.stages.size(), 2u);
}

TEST_F(t_ShaderParser, VertexAndFragment_StageTypesAreCorrect)
{
    const std::string source =
        "#pragma stage vertex\n" + kVertBody +
        "#pragma stage fragment\n" + kFragBody;
    ParseResult result = ParseShaderStages(source);
    ASSERT_EQ(result.stages.size(), 2u);
    EXPECT_EQ(result.stages[0].stage, ShaderStage::Vertex);
    EXPECT_EQ(result.stages[1].stage, ShaderStage::Fragment);
}

TEST_F(t_ShaderParser, MultiStage_LastStageSourceContainsExpectedCode)
{
    // Last stage source is always extracted fully (up to end of file)
    const std::string source =
        "#pragma stage vertex\n" + kVertBody +
        "#pragma stage fragment\nvoid frag() {}\n";
    ParseResult result = ParseShaderStages(source);
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.stages.size(), 2u);
    EXPECT_NE(result.stages[1].glslSource.find("void frag()"), std::string::npos);
}

TEST_F(t_ShaderParser, MultiStage_FirstStageSourceDoesNotContainNextPragma)
{
    // Verifies the stage boundary is exact: the first stage's source must not
    // bleed into the '#pragma stage' line of the next stage.
    const std::string source =
        "#pragma stage vertex\nvoid vert() {}\n"
        "#pragma stage fragment\nvoid frag() {}\n";
    ParseResult result = ParseShaderStages(source);
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.stages.size(), 2u);
    EXPECT_EQ(result.stages[0].glslSource.find("#pragma stage"), std::string::npos);
}

TEST_F(t_ShaderParser, ThreeStages_ReturnCorrectCount)
{
    const std::string source =
        "#pragma stage vertex\nvoid main() {}\n"
        "#pragma stage geometry\nvoid main() {}\n"
        "#pragma stage fragment\nvoid main() {}\n";
    ParseResult result = ParseShaderStages(source);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.stages.size(), 3u);
}

TEST_F(t_ShaderParser, ThreeStages_TypeOrderIsPreserved)
{
    const std::string source =
        "#pragma stage vertex\nvoid main() {}\n"
        "#pragma stage geometry\nvoid main() {}\n"
        "#pragma stage fragment\nvoid main() {}\n";
    ParseResult result = ParseShaderStages(source);
    ASSERT_EQ(result.stages.size(), 3u);
    EXPECT_EQ(result.stages[0].stage, ShaderStage::Vertex);
    EXPECT_EQ(result.stages[1].stage, ShaderStage::Geometry);
    EXPECT_EQ(result.stages[2].stage, ShaderStage::Fragment);
}

// =====================================================
// All Valid Stage Names
// =====================================================

TEST_F(t_ShaderParser, AllValidStageNames_AreRecognized)
{
    const std::vector<std::pair<std::string, ShaderStage>> stagePairs = {
        {"vertex",         ShaderStage::Vertex},
        {"tessControl",    ShaderStage::TessControl},
        {"tessEvaluation", ShaderStage::TessEvaluation},
        {"geometry",       ShaderStage::Geometry},
        {"fragment",       ShaderStage::Fragment},
        {"compute",        ShaderStage::Compute},
    };

    for (const auto& [name, expectedStage] : stagePairs)
    {
        const std::string source = "#pragma stage " + name + "\nvoid main() {}\n";
        ParseResult result = ParseShaderStages(source);
        EXPECT_TRUE(result.success)         << "Stage '" << name << "' should be recognized";
        ASSERT_EQ(result.stages.size(), 1u) << "Stage '" << name << "' should produce 1 stage";
        EXPECT_EQ(result.stages[0].stage, expectedStage) << "Stage type mismatch for '" << name << "'";
    }
}

// =====================================================
// Edge Cases
// =====================================================

TEST_F(t_ShaderParser, WindowsLineEndings_CRLF_DoNotCorruptStageName)
{
    // \r\n line endings should not leave \r in the parsed stage name
    const std::string source = "#pragma stage vertex\r\nvoid main() {}\r\n";
    ParseResult result = ParseShaderStages(source);
    EXPECT_TRUE(result.success);
    ASSERT_EQ(result.stages.size(), 1u);
    EXPECT_EQ(result.stages[0].stage, ShaderStage::Vertex);
}

TEST_F(t_ShaderParser, StageNamesAreCaseSensitive_WrongCaseIsUnknown)
{
    // "Vertex" (capital V) should not be recognized
    const std::string source = "#pragma stage Vertex\nvoid main() {}\n";
    ParseResult result = ParseShaderStages(source);
    EXPECT_FALSE(result.success);
}

TEST_F(t_ShaderParser, MultipleUnknownStages_FailsOnFirstUnknown)
{
    // Parser should fail fast on the first unknown stage encountered
    const std::string source =
        "#pragma stage bad1\nvoid a() {}\n"
        "#pragma stage bad2\nvoid b() {}\n";
    ParseResult result = ParseShaderStages(source);
    EXPECT_FALSE(result.success);
    EXPECT_NE(result.errorMessage.find("bad1"), std::string::npos);
}
