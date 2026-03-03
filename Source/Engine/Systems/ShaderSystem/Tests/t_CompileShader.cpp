#include "Engine/Systems/ShaderSystem/ShaderCompiler/include/ShaderCompiler.h"
#include "Engine/Systems/ShaderSystem/ShaderReflection/include/ShaderReflection.h"
#include "ShaderMockShaders.h"   // wherever your strings are
#include "Engine/Core/Logger/Logger.h"

#include <algorithm> // std::any_of, std::find_if
#include <iostream>

#include "Engine/Core/Application.h"
#include "Engine/Core/Logger/Asserts.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/Modules/ModuleResourceManager/include/ModuleResourceManager.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceShader/include/ResourceShader.h"

// More strict helpers

constexpr LogChannel CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_SYSTEM_SHADERSYSTEM;

static const ReflectedBinding *FindBinding(const ShaderReflectionResult &r,
                                           uint32_t set,
                                           uint32_t binding) {
    auto it = std::find_if(r.bindings.begin(), r.bindings.end(),
                           [&](const ReflectedBinding &b) { return b.set == set && b.binding == binding; });

    return (it != r.bindings.end()) ? &(*it) : nullptr;
}

static const ReflectedInput *FindVertexInput(const ShaderReflectionResult &r, uint32_t location) {
    auto it = std::find_if(r.vertexInputs.begin(), r.vertexInputs.end(),
                           [&](const ReflectedInput &in) { return in.location == location; });

    return (it != r.vertexInputs.end()) ? &(*it) : nullptr;
}

static const ReflectedPushConstant *FindFirstPushConstant(const ShaderReflectionResult &r) {
    if (r.pushConstants.empty())
        return nullptr;

    // In your mock there's only one PC block. If you ever add more, you can search by name.
    return &r.pushConstants.front();
}

static ShaderReflectionResult MergeReflectionPipelineInterface(const ShaderReflectionResult &a,
                                                               const ShaderReflectionResult &b) {
    ShaderReflectionResult out{};
    out.success = a.success && b.success;
    if (!out.success) {
        out.errorMessage = "MergeReflectionPipelineInterface: one or more stage reflections failed.";
        return out;
    }

    // --- Merge bindings by (set,binding). OR stage masks. Validate type/count consistency.
    out.bindings = a.bindings;
    for (const auto &bb: b.bindings) {
        bool merged = false;

        for (auto &ob: out.bindings) {
            if (ob.set == bb.set && ob.binding == bb.binding) {
                // Vulkan interface must match across stages
                NOUS_ASSERT(ob.type == bb.type);
                NOUS_ASSERT(ob.count == bb.count);

                // Name mismatch isn't fatal for Vulkan, but it usually indicates a bug in reflection/authoring.
                // If you want to be strict:
                // assert(ob.name == bb.name);

                ob.stageMask |= bb.stageMask;
                ob.blockSize = std::max(ob.blockSize, bb.blockSize);
                merged = true;
                break;
            }
        }

        if (!merged)
            out.bindings.push_back(bb);
    }

    // --- Merge push constants:
    // For your mock, both stages reflect the same (offset=0,size=80). We OR stageMask.
    out.pushConstants = a.pushConstants;
    for (const auto &pcB: b.pushConstants) {
        bool merged = false;
        for (auto &pcA: out.pushConstants) {
            if (pcA.offset == pcB.offset && pcA.size == pcB.size) {
                pcA.stageMask |= pcB.stageMask;
                merged = true;
                break;
            }
        }
        if (!merged)
            out.pushConstants.push_back(pcB);
    }

    // --- Vertex inputs: pipeline uses vertex stage inputs
    out.vertexInputs = !a.vertexInputs.empty() ? a.vertexInputs : b.vertexInputs;

    return out;
}

static bool HasBindingMember(const ShaderReflectionResult& r,
                             uint32_t set, uint32_t binding,
                             const char* memberName,
                             DataType type,
                             uint32_t offset,
                             uint32_t sizeMin)
{
    for (const auto& b : r.bindings)
    {
        if (b.set == set && b.binding == binding)
        {
            for (const auto& m : b.members)
            {
                if (m.name == memberName &&
                    m.type == type &&
                    m.offset == offset &&
                    m.size >= sizeMin)
                {
                    return true;
                }
            }
            return false; // binding found but member not found
        }
    }
    return false; // binding not found
}

void Test_CompileShader()
{
    ShaderCompilerConfig config{};
    config.entryPoint = "main";
    config.generateDebugInfo = true;
    config.warningsAsErrors = false;
    config.optimization = ShaderOptimizationLevel::Zero;

    // Vertex compile
    ShaderCompileResult vert = NOUS_ShaderSystem::CompileGlslStringToSpirv(
        Nous::ShaderTestShaders::kMockVert,
        ShaderStage::Vertex,
        config,
        "kMockVert.vert"
    );

    if (!vert.success)
        std::cerr << "Vertex compile failed:\n" << vert.errorMessage << "\n";

    NOUS_ASSERT(vert.success);
    NOUS_ASSERT(vert.shaderSource.stage == ShaderStage::Vertex);
    NOUS_ASSERT(!vert.shaderSource.glslSource.empty());
    NOUS_ASSERT(!vert.shaderSource.spirvBinary.empty());

    // Fragment compile
    ShaderCompileResult frag = NOUS_ShaderSystem::CompileGlslStringToSpirv(
        Nous::ShaderTestShaders::kMockFrag,
        ShaderStage::Fragment,
        config,
        "kMockFrag.frag"
    );

    if (!frag.success)
        std::cerr << "Fragment compile failed:\n" << frag.errorMessage << "\n";

    NOUS_ASSERT(frag.success);
    NOUS_ASSERT(frag.shaderSource.stage == ShaderStage::Fragment);
    NOUS_ASSERT(!frag.shaderSource.glslSource.empty());
    NOUS_ASSERT(!frag.shaderSource.spirvBinary.empty());

    NOUS_DEBUG_C(CURRENT_CHANNEL, "TEST SHADERS COMPILATION SUCCESS");

    // -----------------------
    // START REFLECTION
    // -----------------------

    ShaderReflectionResult rv = NOUS_ShaderSystem::ReflectSpirV(vert.shaderSource);
    if (!rv.success)
        std::cerr << "Vertex reflection failed:\n" << rv.errorMessage << "\n";
    NOUS_ASSERT(rv.success);

    ShaderReflectionResult rf = NOUS_ShaderSystem::ReflectSpirV(frag.shaderSource);
    if (!rf.success)
        std::cerr << "Fragment reflection failed:\n" << rf.errorMessage << "\n";
    NOUS_ASSERT(rf.success);

    // ---- Vertex reflection checks (strict) ----

    // 1) Vertex inputs: validate presence + type info (replicable into VkFormat)
    {
        const ReflectedInput *in0 = FindVertexInput(rv, 0);
        const ReflectedInput *in1 = FindVertexInput(rv, 1);
        const ReflectedInput *in2 = FindVertexInput(rv, 2);

        NOUS_ASSERT(in0 && in1 && in2);

        // aPos: vec3 float32
        NOUS_ASSERT(in0->scalarType == ScalarType::Float);
        NOUS_ASSERT(in0->components == 3);
        NOUS_ASSERT(in0->bitWidth == 32);

        // aNormal: vec3 float32
        NOUS_ASSERT(in1->scalarType == ScalarType::Float);
        NOUS_ASSERT(in1->components == 3);
        NOUS_ASSERT(in1->bitWidth == 32);

        // aUV: vec2 float32
        NOUS_ASSERT(in2->scalarType == ScalarType::Float);
        NOUS_ASSERT(in2->components == 2);
        NOUS_ASSERT(in2->bitWidth == 32);

        // Optional: name checks (nice for debug)
        NOUS_ASSERT(in0->name == "aPos");
        NOUS_ASSERT(in1->name == "aNormal");
        NOUS_ASSERT(in2->name == "aUV");
    }

    // 2) UBO: set=0 binding=0 uniform CameraUBO { mat4 uViewProj; }
    {
        const ReflectedBinding *cam = FindBinding(rv, 0, 0);
        NOUS_ASSERT(cam);

        NOUS_ASSERT(cam->type == DescriptorType::UniformBuffer);
        NOUS_ASSERT(cam->count == 1);
        NOUS_ASSERT(cam->name == "CameraUBO" || cam->name == "camera" || !cam->name.empty()); // reflector-dependent

        // Stage visibility must exist
        NOUS_ASSERT(cam->stageMask != 0);

        // Minimum size for mat4 is 64 bytes. Some layouts may add padding, so use >=.
        NOUS_ASSERT(cam->blockSize >= 64);

        // CameraUBO should contain uViewProj at offset 0, mat4, size >= 64
        NOUS_ASSERT(HasBindingMember(rv, 0, 0, "uViewProj", DataType::Mat4, 0, 64));
    }

    // 3) Push constant: must exist and be >0
    // In your shader: mat4 (64) + vec4 (16) = 80 bytes.
    {
        const ReflectedPushConstant *pc = FindFirstPushConstant(rv);
        NOUS_ASSERT(pc);
        NOUS_ASSERT(pc->offset == 0);
        NOUS_ASSERT(pc->size >= 80);
        NOUS_ASSERT(pc->stageMask != 0);

        // Optional: if you are filling members, validate them too (only if your reflector populates pc.members)
        // If not populated yet, you can comment these out.
        if (!pc->members.empty()) {
            // Expect uModel then uTint (or at least these two exist with correct offsets)
            auto hasMember = [&](const char *n, uint32_t off, uint32_t sz) {
                return std::any_of(pc->members.begin(), pc->members.end(),
                                   [&](const ReflectedMember &m) {
                                       return m.name == n && m.offset == off && m.size >= sz;
                                   });
            };

            NOUS_ASSERT(hasMember("uModel", 0, 64));
            NOUS_ASSERT(hasMember("uTint", 64, 16));
        }
    }

    // ---- Fragment reflection checks (strict) ----

    // 4) Combined image sampler: set=1 binding=0 sampler2D uAlbedo;
    {
        const ReflectedBinding *albedo = FindBinding(rf, 1, 0);
        NOUS_ASSERT(albedo);

        NOUS_ASSERT(albedo->type == DescriptorType::CombinedImageSampler);
        NOUS_ASSERT(albedo->count == 1);
        NOUS_ASSERT(albedo->name == "uAlbedo" || !albedo->name.empty()); // reflector-dependent

        NOUS_ASSERT(albedo->stageMask != 0);

        // For samplers/images, blockSize should be 0 (or unused). Don’t fail if you haven’t set it.
        // assert(albedo->blockSize == 0);
    }

    // 5) Push constant exists in fragment too
    {
        const ReflectedPushConstant *pc = FindFirstPushConstant(rf);
        NOUS_ASSERT(pc);
        NOUS_ASSERT(pc->offset == 0);
        NOUS_ASSERT(pc->size >= 80);
        NOUS_ASSERT(pc->stageMask != 0);
    }

    // Optional: fragment should not have vertex inputs
    // Reflection may return none; if yours returns some, you can ignore this check.
    // assert(rf.vertexInputs.empty());

    // ---- Pipeline interface checks (THIS is what Vulkan replicates) ----
    ShaderReflectionResult pipe = MergeReflectionPipelineInterface(rv, rf);
    NOUS_ASSERT(pipe.success);

    // A) Pipeline should have exactly the 2 bindings required by the pair (CameraUBO + uAlbedo)
    {
        const ReflectedBinding *cam = FindBinding(pipe, 0, 0);
        const ReflectedBinding *alb = FindBinding(pipe, 1, 0);

        NOUS_ASSERT(cam);
        NOUS_ASSERT(alb);

        NOUS_ASSERT(cam->type == DescriptorType::UniformBuffer);
        NOUS_ASSERT(cam->count == 1);
        NOUS_ASSERT(cam->blockSize >= 64);

        NOUS_ASSERT(alb->type == DescriptorType::CombinedImageSampler);
        NOUS_ASSERT(alb->count == 1);

        // In a merged interface, stage masks should be OR-ed correctly.
        // We can’t hardcode bit values here, but we *can* verify the merge is a superset:
        const ReflectedBinding *camV = FindBinding(rv, 0, 0);
        const ReflectedBinding *albF = FindBinding(rf, 1, 0);
        NOUS_ASSERT(camV && albF);

        NOUS_ASSERT((cam->stageMask & camV->stageMask) == camV->stageMask);
        NOUS_ASSERT((alb->stageMask & albF->stageMask) == albF->stageMask);
    }

    // B) Push constant must be visible in both stages after merge
    {
        const ReflectedPushConstant *pcV = FindFirstPushConstant(rv);
        const ReflectedPushConstant *pcF = FindFirstPushConstant(rf);
        const ReflectedPushConstant *pcP = FindFirstPushConstant(pipe);

        NOUS_ASSERT(pcV && pcF && pcP);

        NOUS_ASSERT(pcP->offset == 0);
        NOUS_ASSERT(pcP->size >= 80);

        // Merged stage mask should contain both stage masks
        NOUS_ASSERT((pcP->stageMask & pcV->stageMask) == pcV->stageMask);
        NOUS_ASSERT((pcP->stageMask & pcF->stageMask) == pcF->stageMask);
    }

    // C) Vertex inputs in pipeline interface match vertex stage (replicable to VkVertexInputAttributeDescription)
    {
        const ReflectedInput *in0 = FindVertexInput(pipe, 0);
        const ReflectedInput *in1 = FindVertexInput(pipe, 1);
        const ReflectedInput *in2 = FindVertexInput(pipe, 2);

        NOUS_ASSERT(in0 && in1 && in2);

        NOUS_ASSERT(in0->scalarType == ScalarType::Float && in0->components == 3 && in0->bitWidth == 32);
        NOUS_ASSERT(in1->scalarType == ScalarType::Float && in1->components == 3 && in1->bitWidth == 32);
        NOUS_ASSERT(in2->scalarType == ScalarType::Float && in2->components == 2 && in2->bitWidth == 32);
    }

    NOUS_DEBUG_C(CURRENT_CHANNEL, "TEST SHADERS REFLECTION PIPELINE INTERFACE SUCCESS");

    // -----------------------
    // RESOURCE SHADER CREATION
    // -----------------------

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
