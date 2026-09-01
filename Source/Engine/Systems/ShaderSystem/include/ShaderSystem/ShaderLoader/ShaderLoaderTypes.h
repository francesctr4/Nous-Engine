#ifndef NOUS_ENGINE_SHADERLOADERTYPES_H
#define NOUS_ENGINE_SHADERLOADERTYPES_H

#include <ShaderSystem/ShaderTypes.h>
#include <ShaderSystem/ShaderReflection/ShaderReflectionTypes.h>

#include <expected>
#include <string>
#include <vector>

struct RawStage
{
    ShaderStage stage;
    std::string glslSource; // The GLSL text of that stage, with its own #version etc.
};

// The CPU-side product of compiling a shader: the per-stage SPIR-V plus the
// merged pipeline reflection. Deliberately NOT a ResourceShader — no consumer
// ever used the old temporary as one; both moved these two fields out and
// deleted it. Returning a value removes that allocation and its paired frees.
struct CompiledShaderData
{
    std::vector<ShaderSource> stagesData;
    PipelineReflectionResult  reflection;
};

using ShaderLoadResult = std::expected<CompiledShaderData, std::string>;

#endif //NOUS_ENGINE_SHADERLOADERTYPES_H
