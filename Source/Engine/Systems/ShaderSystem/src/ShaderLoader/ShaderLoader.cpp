#include <Logger/Logger.h>
#include <ShaderSystem/ShaderCompiler/ShaderCompiler.h>
#include <ShaderSystem/ShaderLoader/ShaderLoader.h>
#include <FileSystem/FileHandle.h>
#include <ShaderSystem/ShaderLoader/ShaderLoaderTypes.h>
#include <ShaderSystem/ShaderParser/ShaderParser.h>
#include <ShaderSystem/ShaderReflection/ShaderReflection.h>

constexpr auto CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_SYSTEM_SHADERSYSTEM;

// ShaderLoader.cpp
ShaderLoadResult nous::engine::shader_system::LoadShaderFromSource(
    const std::string& fullSource,
    const std::string& debugName,
    const ShaderCompilerConfig& config)
{
    NOUS_DEBUG_C(CURRENT_CHANNEL, "Loading shader from source '%s'", debugName.c_str());

    // 1. Parse stages
    ParseResult parsed = ParseShaderStages(fullSource);
    if (!parsed.has_value())
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "Parse failed for '%s': %s",
                     debugName.c_str(), parsed.error().c_str());
        return std::unexpected(parsed.error());
    }

    // 2. Compile + reflect each stage
    std::vector<ReflectionData> reflections;
    std::vector<ShaderSource>   sources;

    for (const RawStage& raw : *parsed)
    {
        const std::string stageSuffix = raw.stage == ShaderStage::Vertex   ? ".vert"
                                      : raw.stage == ShaderStage::Fragment ? ".frag"
                                      : ".comp";
        const std::string stageName = debugName + stageSuffix;

        ShaderCompileResult compiled = CompileGlslStringToSpirv(
            raw.glslSource, raw.stage, config, stageName);

        if (!compiled.has_value())
        {
            std::string message = "Stage compile failed [" + stageName + "]: " + compiled.error();
            NOUS_ERROR_C(CURRENT_CHANNEL, "%s", message.c_str());
            return std::unexpected(std::move(message));
        }

        ShaderReflectionResult reflected = ReflectSpirV(*compiled);
        if (!reflected.has_value())
        {
            std::string message = "Stage reflection failed [" + stageName + "]: " + reflected.error();
            NOUS_ERROR_C(CURRENT_CHANNEL, "%s", message.c_str());
            return std::unexpected(std::move(message));
        }

        sources.push_back(std::move(*compiled));
        reflections.push_back(std::move(*reflected));
    }

    // 3. Merge
    PipelineReflectionResult pipeline = MergeReflections(reflections);

    // 4. Hand back the CPU-side product by value — no temporary ResourceShader.
    CompiledShaderData data;
    data.reflection = std::move(pipeline);
    data.stagesData = std::move(sources);

    NOUS_DEBUG_C(CURRENT_CHANNEL, "Shader loaded '%s' (%zu stage(s))",
                 debugName.c_str(), data.stagesData.size());

    return data;
}

ShaderLoadResult nous::engine::shader_system::LoadShaderFromFile(
    const std::string& path, const ShaderCompilerConfig& config)
{
    NOUS_DEBUG_C(CURRENT_CHANNEL, "Loading shader from file '%s'", path.c_str());

    FileHandle file;
    if (!file.Open(path, FileMode::READ, true))
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "Cannot open shader file '%s'", path.c_str());
        return std::unexpected("LoadShaderFromFile: cannot open '" + path + "'");
    }

    std::optional<NOUS_Vector<char>> buffer = file.ReadAllBytes();

    if (!buffer.has_value())
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "Failed to read shader file '%s'", path.c_str());
        return std::unexpected("LoadShaderFromFile: failed to read '" + path + "'");
    }

    NOUS_DEBUG_C(CURRENT_CHANNEL, "Read shader file '%s' (%llu bytes)",
                 path.c_str(), static_cast<uint64_t>(buffer->size()));

    const std::string source(buffer->data(), buffer->size());

    return LoadShaderFromSource(source, path, config);
}