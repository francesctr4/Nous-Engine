#include <gtest/gtest.h>

#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/Systems/ShaderSystem/ShaderLoader/include/ShaderLoader.h"
#include "Engine/Systems/ShaderSystem/ShaderLoader/include/ShaderLoaderTypes.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceShader/include/ResourceShader.h"
#include "Engine/Systems/ShaderSystem/ShaderCompiler/include/ShaderCompilerTypes.h"

using namespace NOUS_ShaderSystem;

// =====================================================
// Mock Shaders
// =====================================================

namespace
{
    // Unified vertex + fragment shader (separated by #pragma stage)
    constexpr const char* kUnifiedVertFrag = R"(#pragma stage vertex
#version 450
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 0) out vec2 vUV;
layout(set = 0, binding = 0) uniform CameraUBO { mat4 uViewProj; } camera;
layout(push_constant) uniform Push { mat4 uModel; vec4 uTint; } pc;
void main() { vUV = aUV; gl_Position = camera.uViewProj * pc.uModel * vec4(aPos, 1.0); }
#pragma stage fragment
#version 450
layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;
layout(set = 1, binding = 0) uniform sampler2D uAlbedo;
layout(push_constant) uniform Push { mat4 uModel; vec4 uTint; } pc;
void main() { outColor = texture(uAlbedo, vUV) * pc.uTint; }
)";

    // Single vertex-only shader
    constexpr const char* kUnifiedVertOnly = R"(#pragma stage vertex
#version 450
layout(location = 0) in vec3 aPos;
void main() { gl_Position = vec4(aPos, 1.0); }
)";

    // Source with no #pragma stage directives
    constexpr const char* kNoPragmaSource = R"(
#version 450
void main() { gl_Position = vec4(0.0); }
)";

    // Source with invalid GLSL after the pragma
    constexpr const char* kInvalidGlslSource = R"(#pragma stage vertex
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

// =====================================================
// Fixture - initializes MemoryManager for NOUS_NEW/NOUS_DELETE
// =====================================================

class t_ShaderLoader : public ::testing::Test
{
protected:
    void SetUp() override
    {
        MemoryManager::InitializeMemory(65536);
        config = DefaultConfig();
    }

    void TearDown() override
    {
        MemoryManager::ShutdownMemory();
    }

    ShaderCompilerConfig config;
};

// =====================================================
// LoadShaderFromSource - Success Path (vertex + fragment)
// =====================================================

TEST_F(t_ShaderLoader, UnifiedVertFrag_ReturnsSuccess)
{
    ShaderLoadResult result = LoadShaderFromSource(kUnifiedVertFrag, "MockShader", config);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.errorMessage.empty());
    NOUS_DELETE(result.shader, MemoryTag::RESOURCE_SHADER);
}

TEST_F(t_ShaderLoader, UnifiedVertFrag_ShaderIsNotNull)
{
    ShaderLoadResult result = LoadShaderFromSource(kUnifiedVertFrag, "MockShader", config);
    ASSERT_TRUE(result.success);
    EXPECT_NE(result.shader, nullptr);
    NOUS_DELETE(result.shader, MemoryTag::RESOURCE_SHADER);
}

TEST_F(t_ShaderLoader, UnifiedVertFrag_HasTwoStages)
{
    ShaderLoadResult result = LoadShaderFromSource(kUnifiedVertFrag, "MockShader", config);
    ASSERT_TRUE(result.success);
    ASSERT_NE(result.shader, nullptr);
    EXPECT_EQ(result.shader->stagesData.size(), 2u);
    NOUS_DELETE(result.shader, MemoryTag::RESOURCE_SHADER);
}

TEST_F(t_ShaderLoader, UnifiedVertFrag_FirstStageIsVertex)
{
    ShaderLoadResult result = LoadShaderFromSource(kUnifiedVertFrag, "MockShader", config);
    ASSERT_TRUE(result.success);
    ASSERT_NE(result.shader, nullptr);
    ASSERT_EQ(result.shader->stagesData.size(), 2u);
    EXPECT_EQ(result.shader->stagesData[0].stage, ShaderStage::Vertex);
    NOUS_DELETE(result.shader, MemoryTag::RESOURCE_SHADER);
}

TEST_F(t_ShaderLoader, UnifiedVertFrag_SecondStageIsFragment)
{
    ShaderLoadResult result = LoadShaderFromSource(kUnifiedVertFrag, "MockShader", config);
    ASSERT_TRUE(result.success);
    ASSERT_NE(result.shader, nullptr);
    ASSERT_EQ(result.shader->stagesData.size(), 2u);
    EXPECT_EQ(result.shader->stagesData[1].stage, ShaderStage::Fragment);
    NOUS_DELETE(result.shader, MemoryTag::RESOURCE_SHADER);
}

TEST_F(t_ShaderLoader, UnifiedVertFrag_EachStageHasSpirVBinary)
{
    ShaderLoadResult result = LoadShaderFromSource(kUnifiedVertFrag, "MockShader", config);
    ASSERT_TRUE(result.success);
    ASSERT_NE(result.shader, nullptr);
    for (const ShaderSource& src : result.shader->stagesData)
    {
        EXPECT_FALSE(src.spirvBinary.empty()) << "Stage " << (int)src.stage << " has empty SPIR-V";
    }
    NOUS_DELETE(result.shader, MemoryTag::RESOURCE_SHADER);
}

TEST_F(t_ShaderLoader, UnifiedVertFrag_ReflectionHasTwoDescriptorSets)
{
    ShaderLoadResult result = LoadShaderFromSource(kUnifiedVertFrag, "MockShader", config);
    ASSERT_TRUE(result.success);
    ASSERT_NE(result.shader, nullptr);
    EXPECT_EQ(result.shader->reflection.descriptorSets.size(), 2u);
    NOUS_DELETE(result.shader, MemoryTag::RESOURCE_SHADER);
}

TEST_F(t_ShaderLoader, UnifiedVertFrag_ReflectionHasVertexInputs)
{
    ShaderLoadResult result = LoadShaderFromSource(kUnifiedVertFrag, "MockShader", config);
    ASSERT_TRUE(result.success);
    ASSERT_NE(result.shader, nullptr);
    EXPECT_GE(result.shader->reflection.vertexInputs.size(), 1u);
    NOUS_DELETE(result.shader, MemoryTag::RESOURCE_SHADER);
}

TEST_F(t_ShaderLoader, UnifiedVertFrag_ReflectionHasFragmentOutput)
{
    ShaderLoadResult result = LoadShaderFromSource(kUnifiedVertFrag, "MockShader", config);
    ASSERT_TRUE(result.success);
    ASSERT_NE(result.shader, nullptr);
    EXPECT_GE(result.shader->reflection.fragmentOutputs.size(), 1u);
    NOUS_DELETE(result.shader, MemoryTag::RESOURCE_SHADER);
}

TEST_F(t_ShaderLoader, UnifiedVertFrag_SourcePathIsDebugName)
{
    const std::string debugName = "MockShader";
    ShaderLoadResult result = LoadShaderFromSource(kUnifiedVertFrag, debugName, config);
    ASSERT_TRUE(result.success);
    EXPECT_EQ(result.sourcePath, debugName);
    NOUS_DELETE(result.shader, MemoryTag::RESOURCE_SHADER);
}

// =====================================================
// LoadShaderFromSource - Single Stage
// =====================================================

TEST_F(t_ShaderLoader, SingleVertexStage_ReturnsSuccess)
{
    ShaderLoadResult result = LoadShaderFromSource(kUnifiedVertOnly, "VertOnly", config);
    EXPECT_TRUE(result.success);
    NOUS_DELETE(result.shader, MemoryTag::RESOURCE_SHADER);
}

TEST_F(t_ShaderLoader, SingleVertexStage_HasOneStage)
{
    ShaderLoadResult result = LoadShaderFromSource(kUnifiedVertOnly, "VertOnly", config);
    ASSERT_TRUE(result.success);
    ASSERT_NE(result.shader, nullptr);
    EXPECT_EQ(result.shader->stagesData.size(), 1u);
    NOUS_DELETE(result.shader, MemoryTag::RESOURCE_SHADER);
}

// =====================================================
// LoadShaderFromSource - Error Cases
// =====================================================

TEST_F(t_ShaderLoader, NoPragmaDirectives_ReturnsFailure)
{
    ShaderLoadResult result = LoadShaderFromSource(kNoPragmaSource, "NoPragma", config);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.shader, nullptr);
}

TEST_F(t_ShaderLoader, NoPragmaDirectives_ErrorMessageIsNotEmpty)
{
    ShaderLoadResult result = LoadShaderFromSource(kNoPragmaSource, "NoPragma", config);
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());
}

TEST_F(t_ShaderLoader, EmptySource_ReturnsFailure)
{
    ShaderLoadResult result = LoadShaderFromSource("", "Empty", config);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.shader, nullptr);
}

TEST_F(t_ShaderLoader, InvalidGlsl_ReturnsFailure)
{
    ShaderLoadResult result = LoadShaderFromSource(kInvalidGlslSource, "Invalid", config);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.shader, nullptr);
}

TEST_F(t_ShaderLoader, InvalidGlsl_ErrorMessageIsNotEmpty)
{
    ShaderLoadResult result = LoadShaderFromSource(kInvalidGlslSource, "Invalid", config);
    ASSERT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());
}

// =====================================================
// LoadShaderFromFile - Error Cases
// =====================================================

TEST_F(t_ShaderLoader, LoadFromFile_NonexistentPath_ReturnsFailure)
{
    ShaderLoadResult result = LoadShaderFromFile("nonexistent/path/shader.glsl", config);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.shader, nullptr);
}

TEST_F(t_ShaderLoader, LoadFromFile_NonexistentPath_ErrorMessageIsNotEmpty)
{
    ShaderLoadResult result = LoadShaderFromFile("nonexistent/path/shader.glsl", config);
    ASSERT_FALSE(result.success);
    EXPECT_FALSE(result.errorMessage.empty());
}
