#include "Engine/Systems/ShaderSystem/ShaderCompiler/include/ShaderCompiler.h"
#include "Engine/Systems/ShaderSystem/ShaderReflection/include/ShaderReflection.h"
#include "ShaderMockShaders.h"
#include "Engine/Core/Logger/Logger.h"

#include <algorithm>
#include <iostream>

#include "Engine/Core/Application.h"
#include "Engine/Core/Logger/Asserts.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/Modules/ModuleResourceManager/include/ModuleResourceManager.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceShader/include/ResourceShader.h"
#include "Engine/Systems/ShaderSystem/ShaderLoader/include/ShaderLoader.h"
#include "Engine/Systems/ShaderSystem/ShaderLoader/include/ShaderLoaderTypes.h"

constexpr LogChannel CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_SYSTEM_SHADERSYSTEM;

// ─── Helpers para ShaderReflectionResult (per-stage) ─────────────────────────

static const ReflectedBinding* FindBinding(const ShaderReflectionResult& r,
                                           uint32_t set, uint32_t binding)
{
    auto it = std::find_if(r.bindings.begin(), r.bindings.end(),
        [&](const ReflectedBinding& b) { return b.set == set && b.binding == binding; });
    return (it != r.bindings.end()) ? &(*it) : nullptr;
}

static const ReflectedInput* FindVertexInput(const ShaderReflectionResult& r, uint32_t location)
{
    auto it = std::find_if(r.vertexInputs.begin(), r.vertexInputs.end(),
        [&](const ReflectedInput& in) { return in.location == location; });
    return (it != r.vertexInputs.end()) ? &(*it) : nullptr;
}

static const ReflectedPushConstant* FindFirstPushConstant(const ShaderReflectionResult& r)
{
    return r.pushConstants.empty() ? nullptr : &r.pushConstants.front();
}

static const ReflectedOutput* FindFragmentOutput(const ShaderReflectionResult& r, uint32_t location)
{
    auto it = std::find_if(r.fragmentOutputs.begin(), r.fragmentOutputs.end(),
        [&](const ReflectedOutput& o) { return o.location == location; });
    return (it != r.fragmentOutputs.end()) ? &(*it) : nullptr;
}

static bool HasBindingMember(const ShaderReflectionResult& r,
                             uint32_t set, uint32_t binding,
                             const char* memberName,
                             DataType type, uint32_t offset, uint32_t sizeMin)
{
    for (const auto& b : r.bindings)
    {
        if (b.set != set || b.binding != binding) continue;
        for (const auto& m : b.members)
        {
            if (m.name == memberName && m.type == type &&
                m.offset == offset   && m.size >= sizeMin)
                return true;
        }
        return false;
    }
    return false;
}

// ─── Helpers para PipelineReflectionResult (merged) ──────────────────────────

static const ReflectedBinding* FindBinding(const PipelineReflectionResult& p,
                                           uint32_t set, uint32_t binding)
{
    auto setIt = p.descriptorSets.find(set);
    if (setIt == p.descriptorSets.end()) return nullptr;

    auto it = std::find_if(setIt->second.begin(), setIt->second.end(),
        [&](const ReflectedBinding& b) { return b.binding == binding; });
    return (it != setIt->second.end()) ? &(*it) : nullptr;
}

static const ReflectedInput* FindVertexInput(const PipelineReflectionResult& p, uint32_t location)
{
    auto it = std::find_if(p.vertexInputs.begin(), p.vertexInputs.end(),
        [&](const ReflectedInput& in) { return in.location == location; });
    return (it != p.vertexInputs.end()) ? &(*it) : nullptr;
}

static const ReflectedPushConstant* FindFirstPushConstant(const PipelineReflectionResult& p)
{
    return p.pushConstants.empty() ? nullptr : &p.pushConstants.front();
}

static const ReflectedOutput* FindFragmentOutput(const PipelineReflectionResult& p, uint32_t location)
{
    auto it = std::find_if(p.fragmentOutputs.begin(), p.fragmentOutputs.end(),
        [&](const ReflectedOutput& o) { return o.location == location; });
    return (it != p.fragmentOutputs.end()) ? &(*it) : nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────

void Test_CompileShader()
{
    ShaderCompilerConfig config{};
    config.entryPoint        = "main";
    config.generateDebugInfo = true;
    config.warningsAsErrors  = false;
    config.optimization      = ShaderOptimizationLevel::Zero;

    // ── Compilación ──────────────────────────────────────────────────────────

    ShaderCompileResult vert = NOUS_ShaderSystem::CompileGlslStringToSpirv(
        Nous::ShaderTestShaders::kMockVert, ShaderStage::Vertex, config, "kMockVert.vert");

    if (!vert.success) std::cerr << "Vertex compile failed:\n" << vert.errorMessage << "\n";
    NOUS_ASSERT(vert.success);
    NOUS_ASSERT(vert.shaderSource.stage == ShaderStage::Vertex);
    NOUS_ASSERT(!vert.shaderSource.glslSource.empty());
    NOUS_ASSERT(!vert.shaderSource.spirvBinary.empty());

    ShaderCompileResult frag = NOUS_ShaderSystem::CompileGlslStringToSpirv(
        Nous::ShaderTestShaders::kMockFrag, ShaderStage::Fragment, config, "kMockFrag.frag");

    if (!frag.success) std::cerr << "Fragment compile failed:\n" << frag.errorMessage << "\n";
    NOUS_ASSERT(frag.success);
    NOUS_ASSERT(frag.shaderSource.stage == ShaderStage::Fragment);
    NOUS_ASSERT(!frag.shaderSource.glslSource.empty());
    NOUS_ASSERT(!frag.shaderSource.spirvBinary.empty());

    NOUS_DEBUG_C(CURRENT_CHANNEL, "TEST SHADERS COMPILATION SUCCESS");

    // ── Reflexión por stage ───────────────────────────────────────────────────

    ShaderReflectionResult rv = NOUS_ShaderSystem::ReflectSpirV(vert.shaderSource);
    if (!rv.success) std::cerr << "Vertex reflection failed:\n" << rv.errorMessage << "\n";
    NOUS_ASSERT(rv.success);

    ShaderReflectionResult rf = NOUS_ShaderSystem::ReflectSpirV(frag.shaderSource);
    if (!rf.success) std::cerr << "Fragment reflection failed:\n" << rf.errorMessage << "\n";
    NOUS_ASSERT(rf.success);

    // ── Vertex stage checks ───────────────────────────────────────────────────

    // 1) Vertex inputs: aPos(vec3), aNormal(vec3), aUV(vec2)
    {
        const ReflectedInput* in0 = FindVertexInput(rv, 0);
        const ReflectedInput* in1 = FindVertexInput(rv, 1);
        const ReflectedInput* in2 = FindVertexInput(rv, 2);
        NOUS_ASSERT(in0 && in1 && in2);

        NOUS_ASSERT(in0->name == "aPos");
        NOUS_ASSERT(in0->scalarType  == ScalarType::Float);
        NOUS_ASSERT(in0->components  == 3);
        NOUS_ASSERT(in0->bitWidth    == 32);
        NOUS_ASSERT(in0->ToDataType() == DataType::Vec3);

        NOUS_ASSERT(in1->name == "aNormal");
        NOUS_ASSERT(in1->scalarType  == ScalarType::Float);
        NOUS_ASSERT(in1->components  == 3);
        NOUS_ASSERT(in1->bitWidth    == 32);
        NOUS_ASSERT(in1->ToDataType() == DataType::Vec3);

        NOUS_ASSERT(in2->name == "aUV");
        NOUS_ASSERT(in2->scalarType  == ScalarType::Float);
        NOUS_ASSERT(in2->components  == 2);
        NOUS_ASSERT(in2->bitWidth    == 32);
        NOUS_ASSERT(in2->ToDataType() == DataType::Vec2);
    }

    // 2) CameraUBO: set=0, binding=0
    {
        const ReflectedBinding* cam = FindBinding(rv, 0, 0);
        NOUS_ASSERT(cam);
        NOUS_ASSERT(cam->type      == DescriptorType::UniformBuffer);
        NOUS_ASSERT(cam->count     == 1);
        NOUS_ASSERT(cam->stageMask != 0);
        NOUS_ASSERT(cam->blockSize >= 64);
        NOUS_ASSERT(HasBindingMember(rv, 0, 0, "uViewProj", DataType::Mat4, 0, 64));
    }

    // 3) Push constant vertex: mat4(64) + vec4(16) = 80 bytes
    {
        const ReflectedPushConstant* pc = FindFirstPushConstant(rv);
        NOUS_ASSERT(pc);
        NOUS_ASSERT(pc->offset    == 0);
        NOUS_ASSERT(pc->size      >= 80);
        NOUS_ASSERT(pc->stageMask != 0);

        if (!pc->members.empty())
        {
            auto hasMember = [&](const char* n, uint32_t off, uint32_t sz) {
                return std::any_of(pc->members.begin(), pc->members.end(),
                    [&](const ReflectedMember& m) {
                        return m.name == n && m.offset == off && m.size >= sz;
                    });
            };
            NOUS_ASSERT(hasMember("uModel", 0,  64));
            NOUS_ASSERT(hasMember("uTint",  64, 16));
        }
    }

    // 4) Vertex stage no debe tener fragment outputs
    NOUS_ASSERT(rv.fragmentOutputs.empty());

    // ── Fragment stage checks ─────────────────────────────────────────────────

    // 5) uAlbedo: set=1, binding=0
    {
        const ReflectedBinding* albedo = FindBinding(rf, 1, 0);
        NOUS_ASSERT(albedo);
        NOUS_ASSERT(albedo->type      == DescriptorType::CombinedImageSampler);
        NOUS_ASSERT(albedo->count     == 1);
        NOUS_ASSERT(albedo->stageMask != 0);
        NOUS_ASSERT(albedo->blockSize == 0); // samplers no tienen bloque
    }

    // 6) Push constant fragment
    {
        const ReflectedPushConstant* pc = FindFirstPushConstant(rf);
        NOUS_ASSERT(pc);
        NOUS_ASSERT(pc->offset    == 0);
        NOUS_ASSERT(pc->size      >= 80);
        NOUS_ASSERT(pc->stageMask != 0);
    }

    // 7) Fragment outputs: outColor en location=0 (vec4 float)
    {
        NOUS_ASSERT(!rf.fragmentOutputs.empty());

        const ReflectedOutput* outColor = FindFragmentOutput(rf, 0);
        NOUS_ASSERT(outColor);
        NOUS_ASSERT(outColor->name       == "outColor");
        NOUS_ASSERT(outColor->components == 4);
        NOUS_ASSERT(outColor->bitWidth   == 32);
        NOUS_ASSERT(outColor->scalarType == ScalarType::Float);
    }

    // 8) Fragment stage no debe tener vertex inputs
    NOUS_ASSERT(rf.vertexInputs.empty());

    // ── Pipeline merged checks ────────────────────────────────────────────────

    PipelineReflectionResult pipe = NOUS_ShaderSystem::MergeReflections({rv, rf});

    // A) Bindings merged: CameraUBO(set=0,b=0) + uAlbedo(set=1,b=0)
    {
        const ReflectedBinding* cam    = FindBinding(pipe, 0, 0);
        const ReflectedBinding* albedo = FindBinding(pipe, 1, 0);
        NOUS_ASSERT(cam && albedo);

        NOUS_ASSERT(cam->type      == DescriptorType::UniformBuffer);
        NOUS_ASSERT(cam->count     == 1);
        NOUS_ASSERT(cam->blockSize >= 64);

        NOUS_ASSERT(albedo->type  == DescriptorType::CombinedImageSampler);
        NOUS_ASSERT(albedo->count == 1);

        // stageMask merged es superconjunto de los individuales
        const ReflectedBinding* camV   = FindBinding(rv, 0, 0);
        const ReflectedBinding* albedoF = FindBinding(rf, 1, 0);
        NOUS_ASSERT(camV && albedoF);
        NOUS_ASSERT((cam->stageMask    & camV->stageMask)    == camV->stageMask);
        NOUS_ASSERT((albedo->stageMask & albedoF->stageMask) == albedoF->stageMask);
    }

    // B) Push constant visible en ambos stages tras el merge
    {
        const ReflectedPushConstant* pcV = FindFirstPushConstant(rv);
        const ReflectedPushConstant* pcF = FindFirstPushConstant(rf);
        const ReflectedPushConstant* pcP = FindFirstPushConstant(pipe);
        NOUS_ASSERT(pcV && pcF && pcP);

        NOUS_ASSERT(pcP->offset == 0);
        NOUS_ASSERT(pcP->size   >= 80);
        NOUS_ASSERT((pcP->stageMask & pcV->stageMask) == pcV->stageMask);
        NOUS_ASSERT((pcP->stageMask & pcF->stageMask) == pcF->stageMask);
    }

    // C) Vertex inputs del pipeline coinciden con el stage vertex
    {
        const ReflectedInput* in0 = FindVertexInput(pipe, 0);
        const ReflectedInput* in1 = FindVertexInput(pipe, 1);
        const ReflectedInput* in2 = FindVertexInput(pipe, 2);
        NOUS_ASSERT(in0 && in1 && in2);

        NOUS_ASSERT(in0->ToDataType() == DataType::Vec3);
        NOUS_ASSERT(in1->ToDataType() == DataType::Vec3);
        NOUS_ASSERT(in2->ToDataType() == DataType::Vec2);
    }

    // D) Fragment output propagado al pipeline
    {
        const ReflectedOutput* outColor = FindFragmentOutput(pipe, 0);
        NOUS_ASSERT(outColor);
        NOUS_ASSERT(outColor->components == 4);
        NOUS_ASSERT(outColor->scalarType == ScalarType::Float);
    }

    // E) Número de descriptor sets correcto (set 0 y set 1, ninguno más)
    {
        NOUS_ASSERT(pipe.descriptorSets.size() == 2);
        NOUS_ASSERT(pipe.descriptorSets.count(0) == 1);
        NOUS_ASSERT(pipe.descriptorSets.count(1) == 1);
    }

    NOUS_DEBUG_C(CURRENT_CHANNEL, "TEST SHADERS REFLECTION PIPELINE INTERFACE SUCCESS");

    // ── ResourceShader ────────────────────────────────────────────────────────

    auto* rShader = NOUS_NEW<ResourceShader>(MemoryTag::RESOURCE_SHADER);
    rShader->reflection = pipe;
    rShader->stagesData.push_back(vert.shaderSource);
    rShader->stagesData.push_back(frag.shaderSource);

    NOUS_DEBUG_C(CURRENT_CHANNEL, "TEST SHADERS RESOURCE CREATION SUCCESS");

    NOUS_DELETE(rShader, MemoryTag::RESOURCE_SHADER);

    // TODO: In the end we will need to create our own default shaders like this at the start of the program.
    // TODO: After doing this, defaultShader pointer should have all the info about
    // TODO: ShaderSource, ShaderReflection and IBackendShader, with all the resources already created for Vulkan.
    // TODO: Also, shaders must be being saved in library like any other asset.
    // ResourceShader* defaultMaterialShader =
    //     down_cast<ResourceShader*>(
    //         External->resourceManager->CreateResource(
    //             "Assets/Shaders/BuiltIn.MaterialShader.glsl"));
    //
    // ResourceShader* defaultUIShader =
    //     down_cast<ResourceShader*>(
    //         External->resourceManager->CreateResource(
    //             "Assets/Shaders/BuiltIn.MaterialShader.glsl"));
}

void Test_CompileShaderUnified()
{
    ShaderCompilerConfig config{};
    config.entryPoint   = "main";
    config.optimization = ShaderOptimizationLevel::Zero;

    ShaderLoadResult result = NOUS_ShaderSystem::LoadShaderFromFile(
        "Assets/Shaders/temp_MockShader.glsl", config);

    NOUS_ASSERT(result.success);
    NOUS_ASSERT(result.shader != nullptr);
    NOUS_ASSERT(result.shader->stagesData.size() == 2);

    const PipelineReflectionResult& pipe = result.shader->reflection;

    // A) Descriptor sets: set=0 (CameraUBO) + set=1 (uAlbedo)
    {
        NOUS_ASSERT(pipe.descriptorSets.size() == 2);
        NOUS_ASSERT(pipe.descriptorSets.count(0) == 1);
        NOUS_ASSERT(pipe.descriptorSets.count(1) == 1);

        const ReflectedBinding* cam    = FindBinding(pipe, 0, 0);
        const ReflectedBinding* albedo = FindBinding(pipe, 1, 0);
        NOUS_ASSERT(cam && albedo);

        NOUS_ASSERT(cam->type      == DescriptorType::UniformBuffer);
        NOUS_ASSERT(cam->count     == 1);
        NOUS_ASSERT(cam->blockSize >= 64);
        NOUS_ASSERT(cam->stageMask != 0);

        NOUS_ASSERT(albedo->type      == DescriptorType::CombinedImageSampler);
        NOUS_ASSERT(albedo->count     == 1);
        NOUS_ASSERT(albedo->blockSize == 0);
        NOUS_ASSERT(albedo->stageMask != 0);

        // stageMask de cam solo debe tener vertex, albedo solo fragment
        // No hardcodeamos los bits, pero sí que son distintos entre sí
        NOUS_ASSERT(cam->stageMask != albedo->stageMask);
    }

    // B) Push constant merged visible en ambos stages
    {
        const ReflectedPushConstant* pc = FindFirstPushConstant(pipe);
        NOUS_ASSERT(pc);
        NOUS_ASSERT(pc->offset    == 0);
        NOUS_ASSERT(pc->size      >= 80);
        // stageMask debe tener ambos stages (vertex | fragment)
        NOUS_ASSERT(pc->stageMask != 0);

        if (!pc->members.empty())
        {
            auto hasMember = [&](const char* n, uint32_t off, uint32_t sz) {
                return std::any_of(pc->members.begin(), pc->members.end(),
                    [&](const ReflectedMember& m) {
                        return m.name == n && m.offset == off && m.size >= sz;
                    });
            };
            NOUS_ASSERT(hasMember("uModel", 0,  64));
            NOUS_ASSERT(hasMember("uTint",  64, 16));
        }
    }

    // C) Vertex inputs: aPos(vec3), aNormal(vec3), aUV(vec2)
    {
        const ReflectedInput* in0 = FindVertexInput(pipe, 0);
        const ReflectedInput* in1 = FindVertexInput(pipe, 1);
        const ReflectedInput* in2 = FindVertexInput(pipe, 2);
        NOUS_ASSERT(in0 && in1 && in2);

        NOUS_ASSERT(in0->name == "aPos");
        NOUS_ASSERT(in0->ToDataType() == DataType::Vec3);

        NOUS_ASSERT(in1->name == "aNormal");
        NOUS_ASSERT(in1->ToDataType() == DataType::Vec3);

        NOUS_ASSERT(in2->name == "aUV");
        NOUS_ASSERT(in2->ToDataType() == DataType::Vec2);
    }

    // D) Fragment output: outColor en location=0 (vec4 float)
    {
        NOUS_ASSERT(!pipe.fragmentOutputs.empty());

        const ReflectedOutput* outColor = FindFragmentOutput(pipe, 0);
        NOUS_ASSERT(outColor);
        NOUS_ASSERT(outColor->name       == "outColor");
        NOUS_ASSERT(outColor->components == 4);
        NOUS_ASSERT(outColor->bitWidth   == 32);
        NOUS_ASSERT(outColor->scalarType == ScalarType::Float);
    }

    NOUS_DEBUG_C(CURRENT_CHANNEL, "TEST SHADERS LOAD UNIFIED SUCCESS");

    NOUS_DELETE(result.shader, MemoryTag::RESOURCE_SHADER);
}
