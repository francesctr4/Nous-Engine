#include "Types/ResourceShader/ImporterShader.h"

#include <Logger/Logger.h>
#include <MemoryManager/MemoryManager.h>
#include <FileSystem/FileHandle.h>
#include <FileSystem/FileSystem.h>

#include <ResourceManager/Types/ResourceShader/ResourceShader.h>
#include <ResourceManager/Core/MetaFileData.h>

#include <Renderer/IGPUResourceFactory.h>

// ShaderSystem — using Parser + Compiler + Reflection directly to avoid
// a circular CMake dependency (ShaderLoader returns ResourceShader*).
#include <ShaderSystem/ShaderParser/ShaderParser.h>
#include <ShaderSystem/ShaderCompiler/ShaderCompiler.h>
#include <ShaderSystem/ShaderReflection/ShaderReflection.h>
#include <ShaderSystem/ShaderReflection/ShaderReflectionSerializer.h>

#include <filesystem>
#include <cstring>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const char* StageToFilename(ShaderStage stage)
{
    switch (stage)
    {
        case ShaderStage::Vertex:         return "vertex";
        case ShaderStage::TessControl:    return "tessControl";
        case ShaderStage::TessEvaluation: return "tessEvaluation";
        case ShaderStage::Geometry:       return "geometry";
        case ShaderStage::Fragment:       return "fragment";
        case ShaderStage::Compute:        return "compute";
        default:                          return nullptr;
    }
}

static ShaderStage FilenameToStage(const std::string& stem)
{
    if (stem == "vertex")          return ShaderStage::Vertex;
    if (stem == "tessControl")     return ShaderStage::TessControl;
    if (stem == "tessEvaluation")  return ShaderStage::TessEvaluation;
    if (stem == "geometry")        return ShaderStage::Geometry;
    if (stem == "fragment")        return ShaderStage::Fragment;
    if (stem == "compute")         return ShaderStage::Compute;
    return ShaderStage::Unknown;
}

// libraryPath is "Library\Shaders\<uid>" — the path IS the stage directory.
// replace_extension("") is kept for backward compat with old meta files that stored "<uid>.spv".
static std::string GetShaderDirectory(const std::string& libraryPath)
{
    return std::filesystem::path(libraryPath).replace_extension("").string();
}

// ---------------------------------------------------------------------------
// ImporterShader
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Save helpers
// ---------------------------------------------------------------------------

static bool ReadShaderSource(const std::string& assetsPath, std::string& outSource)
{
    FileHandle glslFile;
    if (!glslFile.Open(assetsPath, FileMode::READ, true))
    {
        NOUS_ERROR("[ImporterShader] Cannot open source '%s'.", assetsPath.c_str());
        return false;
    }

    char*  rawSource = nullptr;
    uint64 bytesRead = 0;
    if (!glslFile.ReadAllBytes(&rawSource, &bytesRead))
    {
        NOUS_ERROR("[ImporterShader] Failed to read '%s'.", assetsPath.c_str());
        glslFile.Close();
        return false;
    }
    glslFile.Close();

    outSource.assign(rawSource, bytesRead);
    NOUS_DELETE(rawSource, MemoryTag::FILE);
    return true;
}

// Compiles every stage in `parsed`, writes each .spv to `shaderDir`, and
// appends the resulting ShaderSource to `outCompiledSources`.
// Optimization is PERFORMANCE by default. The optimizer may strip unused vertex inputs from
// the SPIR-V, but vertex buffer stride and attribute offsets are derived from the actual
// Vertex3D struct layout (not from reflection), so this is safe.
static bool CompileShaderStages(const nous::engine::shader_system::ParseResult& parsed,
                                 const std::string& shaderDir,
                                 const std::string& assetsPath,
                                 std::vector<ShaderSource>& outCompiledSources)
{
    const ShaderCompilerConfig compilerConfig{};
    bool ret = true;

    for (const RawStage& raw : parsed.stages)
    {
        const char* stageName = StageToFilename(raw.stage);
        if (!stageName)
        {
            NOUS_WARN("[ImporterShader] Skipping stage with unrecognized type.");
            continue;
        }

        ShaderCompileResult compiled = nous::engine::shader_system::CompileGlslStringToSpirv(
            raw.glslSource, raw.stage, compilerConfig, assetsPath);

        if (!compiled.success)
        {
            NOUS_ERROR("[ImporterShader] Compile failed for stage '%s': %s",
                       stageName, compiled.errorMessage.c_str());
            ret = false;
            continue;
        }

        const std::string spvPath   = (std::filesystem::path(shaderDir) / stageName).replace_extension(".spv").string();
        const uint64      byteSize  = compiled.shaderSource.spirvBinary.size() * sizeof(uint32_t);

        FileHandle spvFile;
        if (!spvFile.Open(spvPath, FileMode::WRITE, true))
        {
            NOUS_ERROR("[ImporterShader] Cannot open '%s' for writing.", spvPath.c_str());
            ret = false;
            continue;
        }

        uint64 bytesWritten = 0;
        if (!spvFile.Write(byteSize, compiled.shaderSource.spirvBinary.data(), &bytesWritten))
        {
            NOUS_ERROR("[ImporterShader] Failed to write SPIR-V for stage '%s'.", stageName);
            ret = false;
        }
        spvFile.Close();

        NOUS_INFO("[ImporterShader] Saved stage '%s' -> '%s' (%llu bytes)",
                  stageName, spvPath.c_str(), bytesWritten);

        outCompiledSources.push_back(std::move(compiled.shaderSource));
    }

    return ret;
}

// Reflects all compiled stages, merges the results, and writes reflection.json.
static void ReflectAndSerialize(const std::vector<ShaderSource>& compiledSources,
                                 const std::string& shaderDir)
{
    std::vector<ShaderReflectionResult> reflections;
    reflections.reserve(compiledSources.size());

    for (const ShaderSource& src : compiledSources)
    {
        ShaderReflectionResult reflected = nous::engine::shader_system::ReflectSpirV(src);
        if (!reflected.success)
        {
            NOUS_WARN("[ImporterShader] Reflection failed for a stage: %s",
                      reflected.errorMessage.c_str());
        }
        reflections.push_back(std::move(reflected));
    }

    PipelineReflectionResult pipeline = nous::engine::shader_system::MergeReflections(reflections);

    const std::string reflectionJsonPath = (std::filesystem::path(shaderDir) / "reflection.json").string();
    if (!nous::engine::shader_system::SerializeReflection(pipeline, reflectionJsonPath))
    {
        NOUS_WARN("[ImporterShader] Failed to write reflection.json to '%s'.",
                  reflectionJsonPath.c_str());
    }
}

// ---------------------------------------------------------------------------
// ImporterShader
// ---------------------------------------------------------------------------

bool ImporterShader::Import(const MetaFileData& metaFileData)
{
    ResourceBase* tempShader = NOUS_NEW<ResourceShader>(MemoryTag::RESOURCE_SHADER);
    return Save(metaFileData, tempShader);
}

bool ImporterShader::Save(const MetaFileData& metaFileData, ResourceBase*& inResource)
{
    NOUS_DELETE(inResource, MemoryTag::RESOURCE_SHADER);

    std::string source;
    if (!ReadShaderSource(metaFileData.assetsPath, source))
        return false;

    nous::engine::shader_system::ParseResult parsed = nous::engine::shader_system::ParseShaderStages(source);
    if (!parsed.success)
    {
        NOUS_ERROR("[ImporterShader] Parse failed for '%s': %s",
                   metaFileData.assetsPath.c_str(), parsed.errorMessage.c_str());
        return false;
    }

    const std::string shaderDir = GetShaderDirectory(metaFileData.libraryPath);
    if (!nous::engine::filesystem::CreateDirectory(shaderDir))
    {
        NOUS_ERROR("[ImporterShader] Failed to create directory: %s", shaderDir.c_str());
        return false;
    }

    std::vector<ShaderSource> compiledSources;
    if (!CompileShaderStages(parsed, shaderDir, metaFileData.assetsPath, compiledSources))
        return false;

    ReflectAndSerialize(compiledSources, shaderDir);
    return true;
}

bool ImporterShader::Deserialize(const std::string& libraryPath, ResourceBase* outResource)
{
    ResourceShader* shader = down_cast<ResourceShader*>(outResource);

    // 1. Derive stage directory from the library path
    const std::string shaderDir = GetShaderDirectory(libraryPath);

    if (!nous::engine::filesystem::IsDirectory(shaderDir))
    {
        NOUS_ERROR("[ImporterShader] Stage directory not found: %s", shaderDir.c_str());
        return false;
    }

    // 2. Read every .spv in the directory and build ShaderSources
    for (const auto& entry : std::filesystem::directory_iterator(shaderDir))
    {
        if (!entry.is_regular_file())           continue;
        if (entry.path().extension() != ".spv") continue;

        const std::string stem  = entry.path().stem().string();
        const ShaderStage stage = FilenameToStage(stem);

        if (stage == ShaderStage::Unknown)
        {
            NOUS_WARN("[ImporterShader] Unrecognized stage file '%s', skipping.", stem.c_str());
            continue;
        }

        FileHandle file;
        if (!file.Open(entry.path().string(), FileMode::READ, true))
        {
            NOUS_ERROR("[ImporterShader] Cannot open '%s'.", entry.path().string().c_str());
            return false;
        }

        char*  rawBytes  = nullptr;
        uint64 bytesRead = 0;
        if (!file.ReadAllBytes(&rawBytes, &bytesRead))
        {
            NOUS_ERROR("[ImporterShader] Failed to read '%s'.", entry.path().string().c_str());
            file.Close();
            return false;
        }
        file.Close();

        ShaderSource src;
        src.stage = stage;
        src.spirvBinary.resize(bytesRead / sizeof(uint32_t));
        std::memcpy(src.spirvBinary.data(), rawBytes, bytesRead);
        NOUS_DELETE(rawBytes, MemoryTag::FILE);

        shader->stagesData.push_back(std::move(src));
    }

    if (shader->stagesData.empty())
    {
        NOUS_ERROR("[ImporterShader] No SPIR-V stages found in '%s'.", shaderDir.c_str());
        return false;
    }

    // 3. Load reflection: try cached JSON first, fall back to live reflection
    const std::string reflectionJsonPath = (std::filesystem::path(shaderDir) / "reflection.json").string();

    if (!nous::engine::shader_system::DeserializeReflection(reflectionJsonPath, shader->reflection))
    {
        NOUS_WARN("[ImporterShader] reflection.json missing or invalid, reflecting from SPIR-V.");

        std::vector<ShaderReflectionResult> reflections;
        reflections.reserve(shader->stagesData.size());
        for (const ShaderSource& src : shader->stagesData)
        {
            ShaderReflectionResult reflected = nous::engine::shader_system::ReflectSpirV(src);
            if (!reflected.success)
            {
                NOUS_WARN("[ImporterShader] Reflection failed for stage: %s",
                          reflected.errorMessage.c_str());
            }
            reflections.push_back(std::move(reflected));
        }
        shader->reflection = nous::engine::shader_system::MergeReflections(reflections);
    }

    NOUS_INFO("[ImporterShader] Deserialized %zu stage(s) from '%s'.",
              shader->stagesData.size(), shaderDir.c_str());
    return true;
}

bool ImporterShader::Upload(ResourceBase* outResource, IGPUResourceFactory* gpu)
{
    ResourceShader* shader = down_cast<ResourceShader*>(outResource);
    if (!gpu->CreateShader(shader))
    {
        NOUS_ERROR("[ImporterShader] Backend failed to create GPU resources for shader '%s'.",
                   shader->GetName().c_str());
        return false;
    }
    return true;
}

void ImporterShader::Release(ResourceBase* inResource, IGPUResourceFactory* gpu)
{
    ResourceShader* shader = down_cast<ResourceShader*>(inResource);
    gpu->DestroyShader(shader);
}

void ImporterShader::Evict(ResourceBase* inResource)
{
    ResourceShader* shader = down_cast<ResourceShader*>(inResource);
    shader->stagesData.clear();
    shader->reflection = {};
}