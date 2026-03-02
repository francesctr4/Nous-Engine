
#include "Engine/Systems/ShaderSystem/ShaderCompiler/include/ShaderCompiler.h"
#include "Engine/Systems/ShaderSystem/ShaderReflection/include/ShaderReflection.h"
#include "ShaderMockShaders.h"   // wherever your strings are
#include "Engine/Core/Logger/Logger.h"

#include <algorithm> // std::any_of, std::find_if
#include <cassert>
#include <iostream>

// Helpers for reflection checks
static bool HasBinding(const ShaderReflectionResult& r,
                       uint32_t set,
                       uint32_t binding,
                       DescriptorType type,
                       uint32_t count = 1)
{
    return std::any_of(r.bindings.begin(), r.bindings.end(),
        [&](const ReflectedBinding& b)
        {
            return b.set == set &&
                   b.binding == binding &&
                   b.type == type &&
                   b.count == count;
        });
}

static bool HasVertexLocation(const ShaderReflectionResult& r, uint32_t location)
{
    return std::any_of(r.vertexInputs.begin(), r.vertexInputs.end(),
        [&](const ReflectedInput& in)
        {
            return in.location == location;
        });
}

static bool HasAnyPushConstant(const ShaderReflectionResult& r)
{
    return !r.pushConstants.empty() &&
           std::any_of(r.pushConstants.begin(), r.pushConstants.end(),
               [](const ReflectedPushConstant& pc)
               {
                   return pc.size > 0; // offset can be 0
               });
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

    assert(vert.success);
    assert(vert.shaderSource.stage == ShaderStage::Vertex);
    assert(!vert.shaderSource.glslSource.empty());
    assert(!vert.shaderSource.spirvBinary.empty());

    // Fragment compile
    ShaderCompileResult frag = NOUS_ShaderSystem::CompileGlslStringToSpirv(
        Nous::ShaderTestShaders::kMockFrag,
        ShaderStage::Fragment,
        config,
        "kMockFrag.frag"
    );

    if (!frag.success)
        std::cerr << "Fragment compile failed:\n" << frag.errorMessage << "\n";

    assert(frag.success);
    assert(frag.shaderSource.stage == ShaderStage::Fragment);
    assert(!frag.shaderSource.glslSource.empty());
    assert(!frag.shaderSource.spirvBinary.empty());

    NOUS_ERROR("TEST SHADERS COMPILATION SUCCESS");

    // -----------------------
    // START REFLECTION
    // -----------------------

    ShaderReflectionResult rv = NOUS_ShaderSystem::ReflectSpirV(vert.shaderSource);
    if (!rv.success)
        std::cerr << "Vertex reflection failed:\n" << rv.errorMessage << "\n";
    assert(rv.success);

    ShaderReflectionResult rf = NOUS_ShaderSystem::ReflectSpirV(frag.shaderSource);
    if (!rf.success)
        std::cerr << "Fragment reflection failed:\n" << rf.errorMessage << "\n";
    assert(rf.success);

    // ---- Vertex reflection checks ----

    // 1) Vertex inputs (aPos/aNormal/aUV) at locations 0,1,2
    assert(HasVertexLocation(rv, 0));
    assert(HasVertexLocation(rv, 1));
    assert(HasVertexLocation(rv, 2));

    // 2) UBO: layout(set=0, binding=0) uniform CameraUBO { mat4 uViewProj; }
    assert(HasBinding(rv, 0, 0, DescriptorType::UniformBuffer, 1));

    // 3) Push constant exists and has size > 0
    assert(HasAnyPushConstant(rv));

    // ---- Fragment reflection checks ----

    // 4) Combined image sampler: layout(set=1, binding=0) uniform sampler2D uAlbedo;
    assert(HasBinding(rf, 1, 0, DescriptorType::CombinedImageSampler, 1));

    // 5) Push constant exists in fragment too
    assert(HasAnyPushConstant(rf));

    // Optional: sanity check that fragment shouldn't have vertex inputs
    // Some reflectors may return empty; if yours returns inputs for fragment, ignore.
    // assert(rf.vertexInputs.empty());

    NOUS_ERROR("TEST SHADERS REFLECTION SUCCESS");
}