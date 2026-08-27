#include <gtest/gtest.h>

#include <ShaderSystem/ShaderCompiler/ShaderCompiler.h>

using namespace nous::engine::shader_system;

// =====================================================
// Mock Shaders
// =====================================================

namespace
{
    constexpr const char* kMinimalVert = R"(
#version 450
layout(location = 0) in vec3 aPos;
void main() { gl_Position = vec4(aPos, 1.0); }
)";

    constexpr const char* kMinimalFrag = R"(
#version 450
layout(location = 0) out vec4 outColor;
void main() { outColor = vec4(1.0); }
)";

    constexpr const char* kMinimalCompute = R"(
#version 450
layout(local_size_x = 1) in;
void main() {}
)";

    constexpr const char* kInvalidGlsl = R"(
#version 450
THIS IS NOT VALID GLSL ;;;
)";

    ShaderCompilerConfig DefaultConfig()
    {
        ShaderCompilerConfig cfg;
        cfg.entryPoint        = "main";
        cfg.optimization      = ShaderOptimizationLevel::Zero;
        cfg.generateDebugInfo = false;
        cfg.warningsAsErrors  = false;
        return cfg;
    }
}

class t_ShaderCompiler : public ::testing::Test
{
protected:
    ShaderCompilerConfig config = DefaultConfig();
};

// =====================================================
// Vertex Shader - Success Path
// =====================================================

TEST_F(t_ShaderCompiler, ValidVertexGlsl_ReturnsSuccess)
{
    ShaderCompileResult result = CompileGlslStringToSpirv(kMinimalVert, ShaderStage::Vertex, config, "test.vert");
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.errorMessage.empty());
}

TEST_F(t_ShaderCompiler, ValidVertexGlsl_StageIsVertex)
{
    ShaderCompileResult result = CompileGlslStringToSpirv(kMinimalVert, ShaderStage::Vertex, config, "test.vert");
    ASSERT_TRUE(result.success);
    EXPECT_EQ(result.shaderSource.stage, ShaderStage::Vertex);
}

TEST_F(t_ShaderCompiler, ValidVertexGlsl_SpirVBinaryIsNonEmpty)
{
    ShaderCompileResult result = CompileGlslStringToSpirv(kMinimalVert, ShaderStage::Vertex, config, "test.vert");
    ASSERT_TRUE(result.success);
    EXPECT_FALSE(result.shaderSource.spirvBinary.empty());
}

TEST_F(t_ShaderCompiler, ValidVertexGlsl_GlslSourceIsPreserved)
{
    ShaderCompileResult result = CompileGlslStringToSpirv(kMinimalVert, ShaderStage::Vertex, config, "test.vert");
    ASSERT_TRUE(result.success);
    EXPECT_FALSE(result.shaderSource.glslSource.empty());
    EXPECT_NE(result.shaderSource.glslSource.find("aPos"), std::string::npos);
}

// =====================================================
// Fragment Shader - Success Path
// =====================================================

TEST_F(t_ShaderCompiler, ValidFragmentGlsl_ReturnsSuccess)
{
    ShaderCompileResult result = CompileGlslStringToSpirv(kMinimalFrag, ShaderStage::Fragment, config, "test.frag");
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.errorMessage.empty());
}

TEST_F(t_ShaderCompiler, ValidFragmentGlsl_StageIsFragment)
{
    ShaderCompileResult result = CompileGlslStringToSpirv(kMinimalFrag, ShaderStage::Fragment, config, "test.frag");
    ASSERT_TRUE(result.success);
    EXPECT_EQ(result.shaderSource.stage, ShaderStage::Fragment);
}

TEST_F(t_ShaderCompiler, ValidFragmentGlsl_SpirVBinaryIsNonEmpty)
{
    ShaderCompileResult result = CompileGlslStringToSpirv(kMinimalFrag, ShaderStage::Fragment, config, "test.frag");
    ASSERT_TRUE(result.success);
    EXPECT_FALSE(result.shaderSource.spirvBinary.empty());
}

// =====================================================
// Compute Shader
// =====================================================

TEST_F(t_ShaderCompiler, ComputeShader_ReturnsSuccess)
{
    ShaderCompileResult result = CompileGlslStringToSpirv(kMinimalCompute, ShaderStage::Compute, config, "test.comp");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.shaderSource.stage, ShaderStage::Compute);
}

TEST_F(t_ShaderCompiler, ComputeShader_SpirVBinaryIsNonEmpty)
{
    ShaderCompileResult result = CompileGlslStringToSpirv(kMinimalCompute, ShaderStage::Compute, config, "test.comp");
    ASSERT_TRUE(result.success);
    EXPECT_FALSE(result.shaderSource.spirvBinary.empty());
}

// =====================================================
// Error Cases
// =====================================================

TEST_F(t_ShaderCompiler, InvalidGlsl_ReturnsFailure)
{
    ShaderCompileResult result = CompileGlslStringToSpirv(kInvalidGlsl, ShaderStage::Vertex, config, "invalid.vert");
    EXPECT_FALSE(result.success);
}

TEST_F(t_ShaderCompiler, InvalidGlsl_ErrorMessageIsNotEmpty)
{
    ShaderCompileResult result = CompileGlslStringToSpirv(kInvalidGlsl, ShaderStage::Vertex, config, "invalid.vert");
    ASSERT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());
}

TEST_F(t_ShaderCompiler, InvalidGlsl_SpirVBinaryIsEmpty)
{
    ShaderCompileResult result = CompileGlslStringToSpirv(kInvalidGlsl, ShaderStage::Vertex, config, "invalid.vert");
    ASSERT_FALSE(result.success);
    EXPECT_TRUE(result.shaderSource.spirvBinary.empty());
}

TEST_F(t_ShaderCompiler, EmptySource_ReturnsFailure)
{
    ShaderCompileResult result = CompileGlslStringToSpirv("", ShaderStage::Vertex, config, "empty.vert");
    EXPECT_FALSE(result.success);
}

// =====================================================
// SPIR-V Output Properties
// =====================================================

TEST_F(t_ShaderCompiler, SpirVOutput_HasValidMagicNumber)
{
    // SPIR-V magic number is always 0x07230203
    ShaderCompileResult result = CompileGlslStringToSpirv(kMinimalVert, ShaderStage::Vertex, config, "test.vert");
    ASSERT_TRUE(result.success);
    ASSERT_GE(result.shaderSource.spirvBinary.size(), 1u);
    EXPECT_EQ(result.shaderSource.spirvBinary[0], 0x07230203u);
}

TEST_F(t_ShaderCompiler, VirtualPath_IsStoredInShaderSource)
{
    const std::string path = "Assets/Shaders/MyShader.vert";
    ShaderCompileResult result = CompileGlslStringToSpirv(kMinimalVert, ShaderStage::Vertex, config, path);
    ASSERT_TRUE(result.success);
    EXPECT_EQ(result.shaderSource.virtualPath, path);
}

TEST_F(t_ShaderCompiler, ShaderSource_IsValid_AfterSuccessfulCompile)
{
    ShaderCompileResult result = CompileGlslStringToSpirv(kMinimalVert, ShaderStage::Vertex, config, "test.vert");
    ASSERT_TRUE(result.success);
    EXPECT_TRUE(result.shaderSource.IsValid());
}

TEST_F(t_ShaderCompiler, TwoCompilations_ProduceSameBinaryForSameInput)
{
    ShaderCompileResult r1 = CompileGlslStringToSpirv(kMinimalVert, ShaderStage::Vertex, config, "test.vert");
    ShaderCompileResult r2 = CompileGlslStringToSpirv(kMinimalVert, ShaderStage::Vertex, config, "test.vert");
    ASSERT_TRUE(r1.success && r2.success);
    EXPECT_EQ(r1.shaderSource.spirvBinary, r2.shaderSource.spirvBinary);
}

// =====================================================
// Config Options
// =====================================================

TEST_F(t_ShaderCompiler, OptimizationLevelPerformance_CompilesSuccessfully)
{
    ShaderCompilerConfig perfConfig = config;
    perfConfig.optimization = ShaderOptimizationLevel::Performance;
    ShaderCompileResult result = CompileGlslStringToSpirv(kMinimalVert, ShaderStage::Vertex, perfConfig, "test.vert");
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.shaderSource.spirvBinary.empty());
}

TEST_F(t_ShaderCompiler, OptimizationLevelSize_CompilesSuccessfully)
{
    ShaderCompilerConfig sizeConfig = config;
    sizeConfig.optimization = ShaderOptimizationLevel::Size;
    ShaderCompileResult result = CompileGlslStringToSpirv(kMinimalVert, ShaderStage::Vertex, sizeConfig, "test.vert");
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.shaderSource.spirvBinary.empty());
}

TEST_F(t_ShaderCompiler, WarningsAsErrors_CleanGlsl_DoesNotFail)
{
    ShaderCompilerConfig strictConfig = config;
    strictConfig.warningsAsErrors = true;
    ShaderCompileResult result = CompileGlslStringToSpirv(kMinimalVert, ShaderStage::Vertex, strictConfig, "test.vert");
    EXPECT_TRUE(result.success);
}
