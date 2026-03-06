#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/Systems/ShaderSystem/ShaderCompiler/include/ShaderCompiler.h"
#include "Engine/Systems/ShaderSystem/ShaderLoader/include/ShaderLoader.h"
#include "Engine/Core/FileSystem/FileHandle/include/FileHandle.h"
#include "Engine/Systems/ShaderSystem/ShaderLoader/include/ShaderLoaderTypes.h"
#include "Engine/Systems/ShaderSystem/ShaderParser/include/ShaderParser.h"
#include "Engine/Systems/ShaderSystem/ShaderReflection/include/ShaderReflection.h"

// ShaderLoader.cpp
ShaderLoadResult NOUS_ShaderSystem::LoadShaderFromSource(
    const std::string& fullSource,
    const std::string& debugName,
    const ShaderCompilerConfig& config)
{
    ShaderLoadResult out;
    out.sourcePath = debugName;

    // 1. Parsear stages
    ParseResult parsed = ParseShaderStages(fullSource);
    if (!parsed.success)
    {
        out.errorMessage = parsed.errorMessage;
        return out;
    }

    // 2. Compilar + reflejar cada stage
    std::vector<ShaderReflectionResult> reflections;
    std::vector<ShaderSource>           sources;

    for (const RawStage& raw : parsed.stages)
    {
        const std::string stageSuffix = (raw.stage == ShaderStage::Vertex)   ? ".vert"
                                      : (raw.stage == ShaderStage::Fragment) ? ".frag"
                                      : ".comp";

        ShaderCompileResult compiled = CompileGlslStringToSpirv(
            raw.glslSource, raw.stage, config, debugName + stageSuffix);

        if (!compiled.success)
        {
            out.errorMessage = "Stage compile failed [" + debugName + stageSuffix + "]: "
                             + compiled.errorMessage;
            return out;
        }

        ShaderReflectionResult reflected = ReflectSpirV(compiled.shaderSource);
        if (!reflected.success)
        {
            out.errorMessage = "Stage reflection failed [" + debugName + stageSuffix + "]: "
                             + reflected.errorMessage;
            return out;
        }

        sources.push_back(std::move(compiled.shaderSource));
        reflections.push_back(std::move(reflected));
    }

    // 3. Merge
    PipelineReflectionResult pipeline = MergeReflections(reflections);

    // 4. Construir ResourceShader
    auto* rShader = NOUS_NEW<ResourceShader>(MemoryTag::RESOURCE_SHADER);
    rShader->reflection  = std::move(pipeline);
    rShader->stagesData  = std::move(sources);

    out.shader  = rShader;
    out.success = true;
    return out;
}

ShaderLoadResult NOUS_ShaderSystem::LoadShaderFromFile(
    const std::string& path, const ShaderCompilerConfig& config)
{
    ShaderLoadResult err;

    FileHandle file;
    if (!file.Open(path, FileMode::READ, false))
    {
        err.errorMessage = "LoadShaderFromFile: cannot open '" + path + "'";
        return err;
    }

    char*  buffer    = nullptr;
    uint64 bytesRead = 0;

    if (!file.ReadAllBytes(&buffer, &bytesRead))
    {
        err.errorMessage = "LoadShaderFromFile: failed to read '" + path + "'";
        return err;
    }

    std::string source(buffer, bytesRead);
    NOUS_DELETE(buffer, MemoryTag::FILE);

    return LoadShaderFromSource(source, path, config);
}