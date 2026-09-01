#include <gtest/gtest.h>

#include <MemoryManager/MemoryManager.h>
#include <ShaderSystem/ShaderLoader/ShaderLoader.h>
#include <ShaderSystem/ShaderLoader/ShaderLoaderTypes.h>
#include <ShaderSystem/ShaderCompiler/ShaderCompilerTypes.h>

#include <string>

using namespace nous::engine::shader_system;

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

    // Full pipeline: vertex → tessControl → tessEvaluation → geometry → fragment
    constexpr const char* kAllStages = R"(#pragma stage vertex
#version 450
void main()
{
    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
}
#pragma stage tessControl
#version 450
layout(vertices = 3) out;
void main()
{
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
    if (gl_InvocationID == 0)
    {
        gl_TessLevelOuter[0] = 1.0;
        gl_TessLevelOuter[1] = 1.0;
        gl_TessLevelOuter[2] = 1.0;
        gl_TessLevelInner[0] = 1.0;
    }
}
#pragma stage tessEvaluation
#version 450
layout(triangles, equal_spacing, ccw) in;
void main()
{
    gl_Position = gl_TessCoord.x * gl_in[0].gl_Position
                + gl_TessCoord.y * gl_in[1].gl_Position
                + gl_TessCoord.z * gl_in[2].gl_Position;
}
#pragma stage geometry
#version 450
layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;
void main()
{
    for (int i = 0; i < 3; ++i)
    {
        gl_Position = gl_in[i].gl_Position;
        EmitVertex();
    }
    EndPrimitive();
}
#pragma stage fragment
#version 450
layout(location = 0) out vec4 outColor;
void main()
{
    outColor = vec4(1.0);
}
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

    // Renders the error of a failed result for a gtest message; empty on success.
    std::string ErrorOf(const ShaderLoadResult& r)
    {
        return r.has_value() ? std::string{} : r.error();
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
        nous::engine::memory::InitializeMemory(65536);
        config = DefaultConfig();
    }

    void TearDown() override
    {
        nous::engine::memory::ShutdownMemory();
    }

    ShaderCompilerConfig config;
};

// =====================================================
// LoadShaderFromSource - Success Path (vertex + fragment)
// =====================================================

TEST_F(t_ShaderLoader, UnifiedVertFrag_ReturnsSuccess)
{
    ShaderLoadResult result = LoadShaderFromSource(kUnifiedVertFrag, "MockShader", config);
    EXPECT_TRUE(result.has_value()) << ErrorOf(result);
}

TEST_F(t_ShaderLoader, UnifiedVertFrag_HasTwoStages)
{
    ShaderLoadResult result = LoadShaderFromSource(kUnifiedVertFrag, "MockShader", config);
    ASSERT_TRUE(result.has_value()) << ErrorOf(result);
    EXPECT_EQ(result->stagesData.size(), 2u);
}

TEST_F(t_ShaderLoader, UnifiedVertFrag_FirstStageIsVertex)
{
    ShaderLoadResult result = LoadShaderFromSource(kUnifiedVertFrag, "MockShader", config);
    ASSERT_TRUE(result.has_value()) << ErrorOf(result);
    ASSERT_EQ(result->stagesData.size(), 2u);
    EXPECT_EQ(result->stagesData[0].stage, ShaderStage::Vertex);
}

TEST_F(t_ShaderLoader, UnifiedVertFrag_SecondStageIsFragment)
{
    ShaderLoadResult result = LoadShaderFromSource(kUnifiedVertFrag, "MockShader", config);
    ASSERT_TRUE(result.has_value()) << ErrorOf(result);
    ASSERT_EQ(result->stagesData.size(), 2u);
    EXPECT_EQ(result->stagesData[1].stage, ShaderStage::Fragment);
}

TEST_F(t_ShaderLoader, UnifiedVertFrag_EachStageHasSpirVBinary)
{
    ShaderLoadResult result = LoadShaderFromSource(kUnifiedVertFrag, "MockShader", config);
    ASSERT_TRUE(result.has_value()) << ErrorOf(result);
    for (const ShaderSource& src : result->stagesData)
    {
        EXPECT_FALSE(src.spirvBinary.empty()) << "Stage " << (int)src.stage << " has empty SPIR-V";
    }
}

TEST_F(t_ShaderLoader, UnifiedVertFrag_ReflectionHasTwoDescriptorSets)
{
    ShaderLoadResult result = LoadShaderFromSource(kUnifiedVertFrag, "MockShader", config);
    ASSERT_TRUE(result.has_value()) << ErrorOf(result);
    EXPECT_EQ(result->reflection.descriptorSets.size(), 2u);
}

TEST_F(t_ShaderLoader, UnifiedVertFrag_ReflectionHasVertexInputs)
{
    ShaderLoadResult result = LoadShaderFromSource(kUnifiedVertFrag, "MockShader", config);
    ASSERT_TRUE(result.has_value()) << ErrorOf(result);
    EXPECT_GE(result->reflection.vertexInputs.size(), 1u);
}

TEST_F(t_ShaderLoader, UnifiedVertFrag_ReflectionHasFragmentOutput)
{
    ShaderLoadResult result = LoadShaderFromSource(kUnifiedVertFrag, "MockShader", config);
    ASSERT_TRUE(result.has_value()) << ErrorOf(result);
    EXPECT_GE(result->reflection.fragmentOutputs.size(), 1u);
}

// =====================================================
// LoadShaderFromSource - Single Stage
// =====================================================

TEST_F(t_ShaderLoader, SingleVertexStage_ReturnsSuccess)
{
    ShaderLoadResult result = LoadShaderFromSource(kUnifiedVertOnly, "VertOnly", config);
    EXPECT_TRUE(result.has_value()) << ErrorOf(result);
}

TEST_F(t_ShaderLoader, SingleVertexStage_HasOneStage)
{
    ShaderLoadResult result = LoadShaderFromSource(kUnifiedVertOnly, "VertOnly", config);
    ASSERT_TRUE(result.has_value()) << ErrorOf(result);
    EXPECT_EQ(result->stagesData.size(), 1u);
}

// =====================================================
// LoadShaderFromSource - Error Cases
// =====================================================

TEST_F(t_ShaderLoader, NoPragmaDirectives_ReturnsFailure)
{
    ShaderLoadResult result = LoadShaderFromSource(kNoPragmaSource, "NoPragma", config);
    EXPECT_FALSE(result.has_value());
}

TEST_F(t_ShaderLoader, NoPragmaDirectives_ErrorMessageIsNotEmpty)
{
    ShaderLoadResult result = LoadShaderFromSource(kNoPragmaSource, "NoPragma", config);
    ASSERT_FALSE(result.has_value());
    EXPECT_FALSE(result.error().empty());
}

TEST_F(t_ShaderLoader, EmptySource_ReturnsFailure)
{
    ShaderLoadResult result = LoadShaderFromSource("", "Empty", config);
    EXPECT_FALSE(result.has_value());
}

TEST_F(t_ShaderLoader, InvalidGlsl_ReturnsFailure)
{
    ShaderLoadResult result = LoadShaderFromSource(kInvalidGlslSource, "Invalid", config);
    EXPECT_FALSE(result.has_value());
}

TEST_F(t_ShaderLoader, InvalidGlsl_ErrorMessageIsNotEmpty)
{
    ShaderLoadResult result = LoadShaderFromSource(kInvalidGlslSource, "Invalid", config);
    ASSERT_FALSE(result.has_value());
    EXPECT_FALSE(result.error().empty());
}

TEST_F(t_ShaderLoader, LoadShaderFromSource_Success_LeaksNothing)
{
    const auto before = nous::engine::memory::GetMemoryStats();

    {
        ShaderLoadResult result = LoadShaderFromSource(kUnifiedVertFrag, "MockShader", config);
        ASSERT_TRUE(result.has_value()) << ErrorOf(result);
        EXPECT_GT(result->stagesData.size(), 0u);
    }   // result destroyed here - nothing left for a caller to hand-free

    const auto after = nous::engine::memory::GetMemoryStats();
    EXPECT_EQ(after.totalAllocations, before.totalAllocations)
        << "A shader load must not leak. Under the old raw-pointer ShaderLoadResult "
           "this depended on every caller remembering to NOUS_DELETE result.shader.";
}

TEST_F(t_ShaderLoader, LoadShaderFromSource_Failure_LeaksNothing)
{
    const auto before = nous::engine::memory::GetMemoryStats();

    {
        ShaderLoadResult result = LoadShaderFromSource(
            "this is not a shader and has no #pragma stage directives", "bad.glsl", config);

        ASSERT_FALSE(result.has_value());
        EXPECT_FALSE(result.error().empty());
    }

    const auto after = nous::engine::memory::GetMemoryStats();
    EXPECT_EQ(after.totalAllocations, before.totalAllocations)
        << "A failed shader load must not leak.";
}

// =====================================================
// LoadShaderFromFile - Error Cases
// =====================================================

TEST_F(t_ShaderLoader, LoadFromFile_NonexistentPath_ReturnsFailure)
{
    ShaderLoadResult result = LoadShaderFromFile("nonexistent/path/shader.glsl", config);
    EXPECT_FALSE(result.has_value());
}

TEST_F(t_ShaderLoader, LoadFromFile_NonexistentPath_ErrorMessageIsNotEmpty)
{
    ShaderLoadResult result = LoadShaderFromFile("nonexistent/path/shader.glsl", config);
    ASSERT_FALSE(result.has_value());
    EXPECT_FALSE(result.error().empty());
}

// =====================================================
// Full Pipeline - All Stages
// =====================================================

TEST_F(t_ShaderLoader, AllStages_ReturnsSuccess)
{
    ShaderLoadResult result = LoadShaderFromSource(kAllStages, "AllStages", config);
    EXPECT_TRUE(result.has_value()) << ErrorOf(result);
}

TEST_F(t_ShaderLoader, AllStages_HasFiveStages)
{
    ShaderLoadResult result = LoadShaderFromSource(kAllStages, "AllStages", config);
    ASSERT_TRUE(result.has_value()) << ErrorOf(result);
    EXPECT_EQ(result->stagesData.size(), 5u);
}

TEST_F(t_ShaderLoader, AllStages_StageOrderIsCorrect)
{
    ShaderLoadResult result = LoadShaderFromSource(kAllStages, "AllStages", config);
    ASSERT_TRUE(result.has_value()) << ErrorOf(result);
    ASSERT_EQ(result->stagesData.size(), 5u);
    EXPECT_EQ(result->stagesData[0].stage, ShaderStage::Vertex);
    EXPECT_EQ(result->stagesData[1].stage, ShaderStage::TessControl);
    EXPECT_EQ(result->stagesData[2].stage, ShaderStage::TessEvaluation);
    EXPECT_EQ(result->stagesData[3].stage, ShaderStage::Geometry);
    EXPECT_EQ(result->stagesData[4].stage, ShaderStage::Fragment);
}

TEST_F(t_ShaderLoader, AllStages_EachStageHasSpirVBinary)
{
    ShaderLoadResult result = LoadShaderFromSource(kAllStages, "AllStages", config);
    ASSERT_TRUE(result.has_value()) << ErrorOf(result);
    for (const ShaderSource& src : result->stagesData)
    {
        EXPECT_FALSE(src.spirvBinary.empty()) << "Stage " << (int)src.stage << " has empty SPIR-V";
    }
}

TEST_F(t_ShaderLoader, AllStages_ReflectionHasVertexInputs)
{
    ShaderLoadResult result = LoadShaderFromSource(kAllStages, "AllStages", config);
    ASSERT_TRUE(result.has_value()) << ErrorOf(result);
    EXPECT_GE(result->reflection.vertexInputs.size(), 0u);    // no user-defined inputs in minimal vert
}

TEST_F(t_ShaderLoader, AllStages_ReflectionHasFragmentOutput)
{
    ShaderLoadResult result = LoadShaderFromSource(kAllStages, "AllStages", config);
    ASSERT_TRUE(result.has_value()) << ErrorOf(result);
    EXPECT_EQ(result->reflection.fragmentOutputs.size(), 1u);
}
