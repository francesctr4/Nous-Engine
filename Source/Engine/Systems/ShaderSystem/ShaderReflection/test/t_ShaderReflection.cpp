#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>

#include "Engine/Systems/ShaderSystem/ShaderCompiler/include/ShaderCompiler.h"
#include "Engine/Systems/ShaderSystem/ShaderReflection/include/ShaderReflection.h"
#include "Engine/Systems/ShaderSystem/ShaderReflection/include/ShaderReflectionSerializer.h"

using namespace NOUS_ShaderSystem;

// =====================================================
// Mock Shaders
// =====================================================

namespace
{
    // Full vertex shader with UBO, push constant, and 3 vertex inputs
    constexpr const char* kVertGlsl = R"(
#version 450
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 0) out vec2 vUV;
layout(set = 0, binding = 0) uniform CameraUBO { mat4 uViewProj; } camera;
layout(push_constant) uniform Push { mat4 uModel; vec4 uTint; } pc;
void main() { vUV = aUV; gl_Position = camera.uViewProj * pc.uModel * vec4(aPos, 1.0); }
)";

    // Full fragment shader with sampler, push constant, and color output
    constexpr const char* kFragGlsl = R"(
#version 450
layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;
layout(set = 1, binding = 0) uniform sampler2D uAlbedo;
layout(push_constant) uniform Push { mat4 uModel; vec4 uTint; } pc;
void main() { outColor = texture(uAlbedo, vUV) * pc.uTint; }
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

    const ReflectedBinding* FindBindingInResult(const ShaderReflectionResult& r,
                                                uint32_t set, uint32_t binding)
    {
        auto it = std::find_if(r.bindings.begin(), r.bindings.end(),
            [&](const ReflectedBinding& b){ return b.set == set && b.binding == binding; });
        return (it != r.bindings.end()) ? &(*it) : nullptr;
    }

    const ReflectedBinding* FindBindingInPipeline(const PipelineReflectionResult& p,
                                                   uint32_t set, uint32_t binding)
    {
        auto sit = p.descriptorSets.find(set);
        if (sit == p.descriptorSets.end()) return nullptr;
        auto bit = std::find_if(sit->second.begin(), sit->second.end(),
            [&](const ReflectedBinding& b){ return b.binding == binding; });
        return (bit != sit->second.end()) ? &(*bit) : nullptr;
    }
}

// =====================================================
// ReflectSpirV - Vertex Stage
// =====================================================

class t_ShaderReflection : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ShaderCompilerConfig cfg = DefaultConfig();
        vertResult = CompileGlslStringToSpirv(kVertGlsl, ShaderStage::Vertex,   cfg, "test.vert");
        fragResult = CompileGlslStringToSpirv(kFragGlsl, ShaderStage::Fragment, cfg, "test.frag");
        ASSERT_TRUE(vertResult.success) << "Vertex compile failed: " << vertResult.errorMessage;
        ASSERT_TRUE(fragResult.success) << "Fragment compile failed: " << fragResult.errorMessage;
    }

    ShaderCompileResult vertResult;
    ShaderCompileResult fragResult;
};

TEST_F(t_ShaderReflection, VertexShader_ReflectionSucceeds)
{
    ShaderReflectionResult r = ReflectSpirV(vertResult.shaderSource);
    EXPECT_TRUE(r.success);
    EXPECT_TRUE(r.errorMessage.empty());
}

TEST_F(t_ShaderReflection, VertexShader_HasThreeVertexInputs)
{
    ShaderReflectionResult r = ReflectSpirV(vertResult.shaderSource);
    ASSERT_TRUE(r.success);
    EXPECT_EQ(r.vertexInputs.size(), 3u);
}

TEST_F(t_ShaderReflection, VertexShader_Location0_IsVec3Float_Named_aPos)
{
    ShaderReflectionResult r = ReflectSpirV(vertResult.shaderSource);
    ASSERT_TRUE(r.success);
    auto it = std::find_if(r.vertexInputs.begin(), r.vertexInputs.end(),
        [](const ReflectedInput& in){ return in.location == 0; });
    ASSERT_NE(it, r.vertexInputs.end());
    EXPECT_EQ(it->name, "aPos");
    EXPECT_EQ(it->components, 3);
    EXPECT_EQ(it->scalarType, ScalarType::Float);
    EXPECT_EQ(it->ToDataType(), DataType::Vec3);
}

TEST_F(t_ShaderReflection, VertexShader_Location2_IsVec2Float_Named_aUV)
{
    ShaderReflectionResult r = ReflectSpirV(vertResult.shaderSource);
    ASSERT_TRUE(r.success);
    auto it = std::find_if(r.vertexInputs.begin(), r.vertexInputs.end(),
        [](const ReflectedInput& in){ return in.location == 2; });
    ASSERT_NE(it, r.vertexInputs.end());
    EXPECT_EQ(it->name, "aUV");
    EXPECT_EQ(it->components, 2);
    EXPECT_EQ(it->ToDataType(), DataType::Vec2);
}

TEST_F(t_ShaderReflection, VertexShader_HasCameraUBOBinding_Set0_Binding0)
{
    ShaderReflectionResult r = ReflectSpirV(vertResult.shaderSource);
    ASSERT_TRUE(r.success);
    const ReflectedBinding* cam = FindBindingInResult(r, 0, 0);
    ASSERT_NE(cam, nullptr);
    EXPECT_EQ(cam->type, DescriptorType::UniformBuffer);
    EXPECT_GE(cam->blockSize, 64u);
    EXPECT_NE(cam->stageMask, 0u);
}

TEST_F(t_ShaderReflection, VertexShader_HasPushConstant_WithMinimumSize)
{
    ShaderReflectionResult r = ReflectSpirV(vertResult.shaderSource);
    ASSERT_TRUE(r.success);
    ASSERT_FALSE(r.pushConstants.empty());
    EXPECT_GE(r.pushConstants[0].size, 80u);    // mat4(64) + vec4(16)
    EXPECT_EQ(r.pushConstants[0].offset, 0u);
    EXPECT_NE(r.pushConstants[0].stageMask, 0u);
}

TEST_F(t_ShaderReflection, VertexShader_HasNoFragmentOutputs)
{
    ShaderReflectionResult r = ReflectSpirV(vertResult.shaderSource);
    ASSERT_TRUE(r.success);
    EXPECT_TRUE(r.fragmentOutputs.empty());
}

// =====================================================
// ReflectSpirV - Fragment Stage
// =====================================================

TEST_F(t_ShaderReflection, FragmentShader_ReflectionSucceeds)
{
    ShaderReflectionResult r = ReflectSpirV(fragResult.shaderSource);
    EXPECT_TRUE(r.success);
    EXPECT_TRUE(r.errorMessage.empty());
}

TEST_F(t_ShaderReflection, FragmentShader_HasCombinedImageSampler_Set1_Binding0)
{
    ShaderReflectionResult r = ReflectSpirV(fragResult.shaderSource);
    ASSERT_TRUE(r.success);
    const ReflectedBinding* albedo = FindBindingInResult(r, 1, 0);
    ASSERT_NE(albedo, nullptr);
    EXPECT_EQ(albedo->type, DescriptorType::CombinedImageSampler);
    EXPECT_EQ(albedo->count, 1u);
    EXPECT_EQ(albedo->blockSize, 0u);   // samplers have no block
}

TEST_F(t_ShaderReflection, FragmentShader_HasPushConstant_WithMinimumSize)
{
    ShaderReflectionResult r = ReflectSpirV(fragResult.shaderSource);
    ASSERT_TRUE(r.success);
    ASSERT_FALSE(r.pushConstants.empty());
    EXPECT_GE(r.pushConstants[0].size, 80u);
}

TEST_F(t_ShaderReflection, FragmentShader_HasFragmentOutput_Location0_outColor)
{
    ShaderReflectionResult r = ReflectSpirV(fragResult.shaderSource);
    ASSERT_TRUE(r.success);
    ASSERT_FALSE(r.fragmentOutputs.empty());
    auto it = std::find_if(r.fragmentOutputs.begin(), r.fragmentOutputs.end(),
        [](const ReflectedOutput& o){ return o.location == 0; });
    ASSERT_NE(it, r.fragmentOutputs.end());
    EXPECT_EQ(it->name, "outColor");
    EXPECT_EQ(it->components, 4);
    EXPECT_EQ(it->bitWidth, 32);
    EXPECT_EQ(it->scalarType, ScalarType::Float);
}

TEST_F(t_ShaderReflection, FragmentShader_HasNoVertexInputs)
{
    ShaderReflectionResult r = ReflectSpirV(fragResult.shaderSource);
    ASSERT_TRUE(r.success);
    EXPECT_TRUE(r.vertexInputs.empty());
}

TEST_F(t_ShaderReflection, InvalidSpirV_ReturnsFailure)
{
    ShaderSource badSource;
    badSource.stage       = ShaderStage::Vertex;
    badSource.spirvBinary = {0xDEADBEEFu, 0x12345678u};    // not valid SPIR-V
    ShaderReflectionResult r = ReflectSpirV(badSource);
    EXPECT_FALSE(r.success);
    EXPECT_FALSE(r.errorMessage.empty());
}

// =====================================================
// MergeReflections
// =====================================================

TEST_F(t_ShaderReflection, MergeEmptyList_ReturnsEmptyPipelineResult)
{
    PipelineReflectionResult pipe = MergeReflections({});
    EXPECT_TRUE(pipe.descriptorSets.empty());
    EXPECT_TRUE(pipe.pushConstants.empty());
    EXPECT_TRUE(pipe.vertexInputs.empty());
    EXPECT_TRUE(pipe.fragmentOutputs.empty());
}

TEST_F(t_ShaderReflection, MergeSingleVertexStage_HasOneDescriptorSet)
{
    ShaderReflectionResult rv = ReflectSpirV(vertResult.shaderSource);
    ASSERT_TRUE(rv.success);
    PipelineReflectionResult pipe = MergeReflections({rv});
    EXPECT_EQ(pipe.descriptorSets.size(), 1u);
    EXPECT_EQ(pipe.descriptorSets.count(0), 1u);
}

TEST_F(t_ShaderReflection, MergeSingleVertexStage_VertexInputCountPreserved)
{
    ShaderReflectionResult rv = ReflectSpirV(vertResult.shaderSource);
    ASSERT_TRUE(rv.success);
    PipelineReflectionResult pipe = MergeReflections({rv});
    EXPECT_EQ(pipe.vertexInputs.size(), rv.vertexInputs.size());
}

TEST_F(t_ShaderReflection, MergeVertexAndFragment_HasTwoDescriptorSets)
{
    ShaderReflectionResult rv = ReflectSpirV(vertResult.shaderSource);
    ShaderReflectionResult rf = ReflectSpirV(fragResult.shaderSource);
    ASSERT_TRUE(rv.success && rf.success);
    PipelineReflectionResult pipe = MergeReflections({rv, rf});
    EXPECT_EQ(pipe.descriptorSets.size(), 2u);
    EXPECT_EQ(pipe.descriptorSets.count(0), 1u);
    EXPECT_EQ(pipe.descriptorSets.count(1), 1u);
}

TEST_F(t_ShaderReflection, MergeVertexAndFragment_CameraUBOBinding_IsPresent)
{
    ShaderReflectionResult rv = ReflectSpirV(vertResult.shaderSource);
    ShaderReflectionResult rf = ReflectSpirV(fragResult.shaderSource);
    ASSERT_TRUE(rv.success && rf.success);
    PipelineReflectionResult pipe = MergeReflections({rv, rf});
    const ReflectedBinding* cam = FindBindingInPipeline(pipe, 0, 0);
    ASSERT_NE(cam, nullptr);
    EXPECT_EQ(cam->type, DescriptorType::UniformBuffer);
    EXPECT_GE(cam->blockSize, 64u);
}

TEST_F(t_ShaderReflection, MergeVertexAndFragment_AlbedoSampler_IsPresent)
{
    ShaderReflectionResult rv = ReflectSpirV(vertResult.shaderSource);
    ShaderReflectionResult rf = ReflectSpirV(fragResult.shaderSource);
    ASSERT_TRUE(rv.success && rf.success);
    PipelineReflectionResult pipe = MergeReflections({rv, rf});
    const ReflectedBinding* albedo = FindBindingInPipeline(pipe, 1, 0);
    ASSERT_NE(albedo, nullptr);
    EXPECT_EQ(albedo->type, DescriptorType::CombinedImageSampler);
    EXPECT_EQ(albedo->count, 1u);
}

TEST_F(t_ShaderReflection, MergeVertexAndFragment_PushConstantStageMask_CoversBothStages)
{
    ShaderReflectionResult rv = ReflectSpirV(vertResult.shaderSource);
    ShaderReflectionResult rf = ReflectSpirV(fragResult.shaderSource);
    ASSERT_TRUE(rv.success && rf.success);
    ASSERT_FALSE(rv.pushConstants.empty());
    ASSERT_FALSE(rf.pushConstants.empty());

    PipelineReflectionResult pipe = MergeReflections({rv, rf});
    ASSERT_FALSE(pipe.pushConstants.empty());

    const uint32_t mergedMask = pipe.pushConstants[0].stageMask;
    const uint32_t vertMask   = rv.pushConstants[0].stageMask;
    const uint32_t fragMask   = rf.pushConstants[0].stageMask;
    EXPECT_EQ(mergedMask & vertMask, vertMask);
    EXPECT_EQ(mergedMask & fragMask, fragMask);
}

TEST_F(t_ShaderReflection, MergeVertexAndFragment_VertexInputs_Preserved)
{
    ShaderReflectionResult rv = ReflectSpirV(vertResult.shaderSource);
    ShaderReflectionResult rf = ReflectSpirV(fragResult.shaderSource);
    ASSERT_TRUE(rv.success && rf.success);
    PipelineReflectionResult pipe = MergeReflections({rv, rf});
    EXPECT_EQ(pipe.vertexInputs.size(), 3u);
}

TEST_F(t_ShaderReflection, MergeVertexAndFragment_FragmentOutputs_Preserved)
{
    ShaderReflectionResult rv = ReflectSpirV(vertResult.shaderSource);
    ShaderReflectionResult rf = ReflectSpirV(fragResult.shaderSource);
    ASSERT_TRUE(rv.success && rf.success);
    PipelineReflectionResult pipe = MergeReflections({rv, rf});
    EXPECT_EQ(pipe.fragmentOutputs.size(), 1u);
}

// =====================================================
// SerializeReflection / DeserializeReflection
// =====================================================

class t_ShaderReflectionSerializer : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ShaderCompilerConfig cfg = DefaultConfig();
        ShaderCompileResult vert = CompileGlslStringToSpirv(kVertGlsl, ShaderStage::Vertex,   cfg, "test.vert");
        ShaderCompileResult frag = CompileGlslStringToSpirv(kFragGlsl, ShaderStage::Fragment, cfg, "test.frag");
        ASSERT_TRUE(vert.success && frag.success);

        ShaderReflectionResult rv = ReflectSpirV(vert.shaderSource);
        ShaderReflectionResult rf = ReflectSpirV(frag.shaderSource);
        ASSERT_TRUE(rv.success && rf.success);

        pipeline = MergeReflections({rv, rf});
        tempJson = (std::filesystem::temp_directory_path() / "nous_test_reflection.json").string();
    }

    void TearDown() override
    {
        std::filesystem::remove(tempJson);
    }

    PipelineReflectionResult pipeline;
    std::string              tempJson;
};

TEST_F(t_ShaderReflectionSerializer, Serialize_ReturnsTrue)
{
    EXPECT_TRUE(SerializeReflection(pipeline, tempJson));
}

TEST_F(t_ShaderReflectionSerializer, Deserialize_AfterSerialize_ReturnsTrue)
{
    ASSERT_TRUE(SerializeReflection(pipeline, tempJson));
    PipelineReflectionResult loaded;
    EXPECT_TRUE(DeserializeReflection(tempJson, loaded));
}

TEST_F(t_ShaderReflectionSerializer, RoundTrip_DescriptorSetCountMatches)
{
    ASSERT_TRUE(SerializeReflection(pipeline, tempJson));
    PipelineReflectionResult loaded;
    ASSERT_TRUE(DeserializeReflection(tempJson, loaded));
    EXPECT_EQ(loaded.descriptorSets.size(), pipeline.descriptorSets.size());
}

TEST_F(t_ShaderReflectionSerializer, RoundTrip_Set0_BindingCountMatches)
{
    ASSERT_TRUE(SerializeReflection(pipeline, tempJson));
    PipelineReflectionResult loaded;
    ASSERT_TRUE(DeserializeReflection(tempJson, loaded));
    ASSERT_EQ(loaded.descriptorSets.count(0), 1u);
    EXPECT_EQ(loaded.descriptorSets.at(0).size(), pipeline.descriptorSets.at(0).size());
}

TEST_F(t_ShaderReflectionSerializer, RoundTrip_PushConstantCountMatches)
{
    ASSERT_TRUE(SerializeReflection(pipeline, tempJson));
    PipelineReflectionResult loaded;
    ASSERT_TRUE(DeserializeReflection(tempJson, loaded));
    EXPECT_EQ(loaded.pushConstants.size(), pipeline.pushConstants.size());
}

TEST_F(t_ShaderReflectionSerializer, RoundTrip_PushConstantFieldsMatch)
{
    ASSERT_TRUE(SerializeReflection(pipeline, tempJson));
    PipelineReflectionResult loaded;
    ASSERT_TRUE(DeserializeReflection(tempJson, loaded));
    ASSERT_FALSE(loaded.pushConstants.empty());
    EXPECT_EQ(loaded.pushConstants[0].size,      pipeline.pushConstants[0].size);
    EXPECT_EQ(loaded.pushConstants[0].offset,    pipeline.pushConstants[0].offset);
    EXPECT_EQ(loaded.pushConstants[0].stageMask, pipeline.pushConstants[0].stageMask);
}

TEST_F(t_ShaderReflectionSerializer, RoundTrip_VertexInputCountMatches)
{
    ASSERT_TRUE(SerializeReflection(pipeline, tempJson));
    PipelineReflectionResult loaded;
    ASSERT_TRUE(DeserializeReflection(tempJson, loaded));
    EXPECT_EQ(loaded.vertexInputs.size(), pipeline.vertexInputs.size());
}

TEST_F(t_ShaderReflectionSerializer, RoundTrip_FragmentOutputCountMatches)
{
    ASSERT_TRUE(SerializeReflection(pipeline, tempJson));
    PipelineReflectionResult loaded;
    ASSERT_TRUE(DeserializeReflection(tempJson, loaded));
    EXPECT_EQ(loaded.fragmentOutputs.size(), pipeline.fragmentOutputs.size());
}

TEST_F(t_ShaderReflectionSerializer, RoundTrip_CameraUBOBlockSizeIsPreserved)
{
    ASSERT_TRUE(SerializeReflection(pipeline, tempJson));
    PipelineReflectionResult loaded;
    ASSERT_TRUE(DeserializeReflection(tempJson, loaded));
    ASSERT_EQ(loaded.descriptorSets.count(0), 1u);
    const auto& bindings = loaded.descriptorSets.at(0);
    auto it = std::find_if(bindings.begin(), bindings.end(),
        [](const ReflectedBinding& b){ return b.binding == 0; });
    ASSERT_NE(it, bindings.end());
    EXPECT_GE(it->blockSize, 64u);
    EXPECT_EQ(it->type, DescriptorType::UniformBuffer);
}

TEST_F(t_ShaderReflectionSerializer, Deserialize_NonexistentFile_ReturnsFalse)
{
    PipelineReflectionResult loaded;
    EXPECT_FALSE(DeserializeReflection("/nonexistent/path/to/reflection.json", loaded));
}
