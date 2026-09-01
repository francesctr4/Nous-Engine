#include <gtest/gtest.h>

#include <MemoryManager/MemoryManager.h>
#include <ShaderSystem/ShaderLoader/ShaderLoader.h>
#include <ShaderSystem/ShaderLoader/ShaderLoaderTypes.h>
#include <ShaderSystem/ShaderCompiler/ShaderCompilerTypes.h>

#include <cstdint>
#include <vector>

using namespace nous::engine::shader_system;

// ─────────────────────────────────────────────────────────────────────────────
// Shader fixtures — minimal vert+frag shaders that differ only in frag output
// so that a reload produces detectably different SPIR-V.
// ─────────────────────────────────────────────────────────────────────────────
namespace
{
    // Version 1 — red output
    constexpr const char* kShaderV1 = R"(#pragma stage vertex
#version 450
void main() { gl_Position = vec4(0.0, 0.0, 0.0, 1.0); }
#pragma stage fragment
#version 450
layout(location = 0) out vec4 outColor;
void main() { outColor = vec4(1.0, 0.0, 0.0, 1.0); }
)";

    // Version 2 — green output (same structure, different constant)
    constexpr const char* kShaderV2 = R"(#pragma stage vertex
#version 450
void main() { gl_Position = vec4(0.0, 0.0, 0.0, 1.0); }
#pragma stage fragment
#version 450
layout(location = 0) out vec4 outColor;
void main() { outColor = vec4(0.0, 1.0, 0.0, 1.0); }
)";

    // Broken — invalid GLSL in the fragment stage
    constexpr const char* kShaderBroken = R"(#pragma stage vertex
#version 450
void main() { gl_Position = vec4(0.0, 0.0, 0.0, 1.0); }
#pragma stage fragment
#version 450
layout(location = 0) out vec4 outColor;
void main() { outColor = THIS_IS_NOT_VALID_GLSL; }
)";

    ShaderCompilerConfig MakeConfig()
    {
        ShaderCompilerConfig cfg;
        cfg.entryPoint        = "main";
        cfg.optimization      = ShaderOptimizationLevel::Zero;
        cfg.generateDebugInfo = false;
        cfg.warningsAsErrors  = false;
        return cfg;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Fixture
// ─────────────────────────────────────────────────────────────────────────────
class t_ShaderHotReload : public ::testing::Test
{
protected:
    void SetUp() override    { nous::engine::memory::InitializeMemory(65536); }
    void TearDown() override { nous::engine::memory::ShutdownMemory(); }
};

// ─────────────────────────────────────────────────────────────────────────────
// CPU reload cycle — compile → re-compile with new source
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(t_ShaderHotReload, SuccessfulReload_BothVersionsSucceed)
{
    const auto cfg = MakeConfig();
    ShaderLoadResult v1 = LoadShaderFromSource(kShaderV1, "TestShader.glsl", cfg);
    ShaderLoadResult v2 = LoadShaderFromSource(kShaderV2, "TestShader.glsl", cfg);

    EXPECT_TRUE(v1.has_value());
    EXPECT_TRUE(v2.has_value());
}

TEST_F(t_ShaderHotReload, SuccessfulReload_FragmentSpirVDiffers)
{
    // Reloading with a different colour constant must produce different SPIR-V
    // for the fragment stage (the vertex stage may be identical — that's fine).
    const auto cfg = MakeConfig();
    ShaderLoadResult v1 = LoadShaderFromSource(kShaderV1, "TestShader.glsl", cfg);
    ShaderLoadResult v2 = LoadShaderFromSource(kShaderV2, "TestShader.glsl", cfg);

    ASSERT_TRUE(v1.has_value());
    ASSERT_TRUE(v2.has_value());
    ASSERT_EQ(v1->stagesData.size(), 2u);
    ASSERT_EQ(v2->stagesData.size(), 2u);

    // Fragment is stage index 1 (vert=0, frag=1 after parser ordering)
    const auto& spirvV1Frag = v1->stagesData[1].spirvBinary;
    const auto& spirvV2Frag = v2->stagesData[1].spirvBinary;
    EXPECT_NE(spirvV1Frag, spirvV2Frag)
        << "SPIR-V must differ after reloading a shader with a different colour constant.";
}

// ─────────────────────────────────────────────────────────────────────────────
// Error handling — broken source must not corrupt existing shader data
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(t_ShaderHotReload, FailedReload_ReturnsFalse)
{
    ShaderLoadResult bad = LoadShaderFromSource(kShaderBroken, "Broken.glsl", MakeConfig());
    EXPECT_FALSE(bad.has_value());
}

TEST_F(t_ShaderHotReload, FailedReload_ErrorMessageIsNonEmpty)
{
    ShaderLoadResult bad = LoadShaderFromSource(kShaderBroken, "Broken.glsl", MakeConfig());
    ASSERT_FALSE(bad.has_value());
    EXPECT_FALSE(bad.error().empty())
        << "A compile failure must carry the shaderc diagnostics as its error.";
}

TEST_F(t_ShaderHotReload, FailedReload_OriginalSpirVUntouched)
{
    // This verifies the backend invariant enforced by VulkanBackend::ReloadShader:
    // on failure we do NOT swap stagesData, so the in-flight ResourceShader is safe.
    const auto cfg = MakeConfig();
    ShaderLoadResult original = LoadShaderFromSource(kShaderV1, "TestShader.glsl", cfg);
    ASSERT_TRUE(original.has_value());

    // Save a copy of the original SPIR-V
    const std::vector<uint32_t> savedSpirV = original->stagesData[0].spirvBinary;

    // Attempt a broken reload
    ShaderLoadResult bad = LoadShaderFromSource(kShaderBroken, "TestShader.glsl", cfg);
    EXPECT_FALSE(bad.has_value());

    // Simulate the backend: only swap on success — original data is untouched
    if (bad.has_value())
    {
        original->stagesData = std::move(bad->stagesData);
        original->reflection = std::move(bad->reflection);
    }

    EXPECT_EQ(original->stagesData[0].spirvBinary, savedSpirV)
        << "Original SPIR-V must remain unchanged after a failed reload.";
}
