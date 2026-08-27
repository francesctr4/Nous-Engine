#include <Logger/Logger.h>
#include <MemoryManager/MemoryManager.h>
#include <ShaderSystem/ShaderCompiler/ShaderCompiler.h>
#include <ShaderSystem/ShaderLoader/ShaderLoader.h>
#include <FileSystem/FileHandle.h>
#include <ShaderSystem/ShaderLoader/ShaderLoaderTypes.h>
#include <ShaderSystem/ShaderParser/ShaderParser.h>
#include <ShaderSystem/ShaderReflection/ShaderReflection.h>
#include "Engine/Systems/ResourceManager/Types/ResourceShader/include/ResourceShader.h"

constexpr auto CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_SYSTEM_SHADERSYSTEM;

// ShaderLoader.cpp
ShaderLoadResult nous::engine::shader_system::LoadShaderFromSource(
    const std::string& fullSource,
    const std::string& debugName,
    const ShaderCompilerConfig& config)
{
    NOUS_DEBUG_C(CURRENT_CHANNEL, "Loading shader from source '%s'", debugName.c_str());

    ShaderLoadResult out;
    out.sourcePath = debugName;

    // 1. Parse stages
    ParseResult parsed = ParseShaderStages(fullSource);
    if (!parsed.success)
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "Parse failed for '%s': %s",
                     debugName.c_str(), parsed.errorMessage.c_str());
        out.errorMessage = parsed.errorMessage;
        return out;
    }

    // 2. Compile + reflect each stage
    std::vector<ShaderReflectionResult> reflections;
    std::vector<ShaderSource>           sources;

    for (const RawStage& raw : parsed.stages)
    {
        const std::string stageSuffix = raw.stage == ShaderStage::Vertex   ? ".vert"
                                      : raw.stage == ShaderStage::Fragment ? ".frag"
                                      : ".comp";
        const std::string stageName = debugName + stageSuffix;

        ShaderCompileResult compiled = CompileGlslStringToSpirv(
            raw.glslSource, raw.stage, config, stageName);

        if (!compiled.success)
        {
            out.errorMessage = "Stage compile failed [" + stageName + "]: " + compiled.errorMessage;
            NOUS_ERROR_C(CURRENT_CHANNEL, "%s", out.errorMessage.c_str());
            return out;
        }

        ShaderReflectionResult reflected = ReflectSpirV(compiled.shaderSource);
        if (!reflected.success)
        {
            out.errorMessage = "Stage reflection failed [" + stageName + "]: " + reflected.errorMessage;
            NOUS_ERROR_C(CURRENT_CHANNEL, "%s", out.errorMessage.c_str());
            return out;
        }

        sources.push_back(std::move(compiled.shaderSource));
        reflections.push_back(std::move(reflected));
    }

    // 3. Merge
    PipelineReflectionResult pipeline = MergeReflections(reflections);

    // 4. Build ResourceShader
    auto* rShader = NOUS_NEW<ResourceShader>(MemoryTag::RESOURCE_SHADER);
    rShader->reflection  = std::move(pipeline);
    rShader->stagesData  = std::move(sources);

    out.shader  = rShader;
    out.success = true;

    NOUS_DEBUG_C(CURRENT_CHANNEL, "Shader loaded '%s' (%zu stage(s))",
                 debugName.c_str(), rShader->stagesData.size());
    return out;
}

ShaderLoadResult nous::engine::shader_system::LoadShaderFromFile(
    const std::string& path, const ShaderCompilerConfig& config)
{
    NOUS_DEBUG_C(CURRENT_CHANNEL, "Loading shader from file '%s'", path.c_str());

    ShaderLoadResult err;

    FileHandle file;
    if (!file.Open(path, FileMode::READ, true))
    {
        err.errorMessage = "LoadShaderFromFile: cannot open '" + path + "'";
        NOUS_ERROR_C(CURRENT_CHANNEL, "Cannot open shader file '%s'", path.c_str());
        return err;
    }

    char*  buffer    = nullptr;
    uint64 bytesRead = 0;

    if (!file.ReadAllBytes(&buffer, &bytesRead))
    {
        err.errorMessage = "LoadShaderFromFile: failed to read '" + path + "'";
        NOUS_ERROR_C(CURRENT_CHANNEL, "Failed to read shader file '%s'", path.c_str());
        return err;
    }

    NOUS_DEBUG_C(CURRENT_CHANNEL, "Read shader file '%s' (%llu bytes)", path.c_str(), bytesRead);

    const std::string source(buffer, bytesRead);
    NOUS_DELETE(buffer, MemoryTag::FILE);

    return LoadShaderFromSource(source, path, config);
}