#include <ShaderSystem/ShaderCompiler/ShaderCompiler.h>
#include <Logger/Logger.h>

constexpr auto CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_SYSTEM_SHADERSYSTEM;

#include <shaderc/shaderc.hpp>

#include <fstream>
#include <string>
#include <vector>
#include <cstddef>

namespace
{
    bool ReadTextFile(const std::string& path, std::string& out)
    {
        std::ifstream f(path, std::ios::binary);
        if (!f) return false;

        out.assign(std::istreambuf_iterator(f),
                   std::istreambuf_iterator<char>());
        return true;
    }

    bool WriteBinaryFile(const std::string& path, const void* data, const size_t size)
    {
        std::ofstream f(path, std::ios::binary);
        if (!f) return false;

        f.write(static_cast<const char*>(data),
                static_cast<std::streamsize>(size));
        return f.good();
    }

    shaderc_shader_kind ToShadercKind(const ShaderStage stage)
    {
        switch (stage)
        {
            case ShaderStage::Vertex:         return shaderc_vertex_shader;
            case ShaderStage::Fragment:       return shaderc_fragment_shader;
            case ShaderStage::Compute:        return shaderc_compute_shader;
            case ShaderStage::Geometry:       return shaderc_geometry_shader;
            case ShaderStage::TessControl:    return shaderc_tess_control_shader;
            case ShaderStage::TessEvaluation: return shaderc_tess_evaluation_shader;
            default:                          return shaderc_glsl_infer_from_source;
        }
    }

    shaderc_optimization_level ToShadercOpt(const ShaderOptimizationLevel opt)
    {
        switch (opt)
        {
            case ShaderOptimizationLevel::Zero:        return shaderc_optimization_level_zero;
            case ShaderOptimizationLevel::Size:        return shaderc_optimization_level_size;
            case ShaderOptimizationLevel::Performance:
            default:                                   return shaderc_optimization_level_performance;
        }
    }

    shaderc_shader_kind InferKindFromExtension(const std::string& path)
    {
        if (path.ends_with(".vert.glsl")) return shaderc_vertex_shader;
        if (path.ends_with(".frag.glsl")) return shaderc_fragment_shader;
        if (path.ends_with(".comp.glsl")) return shaderc_compute_shader;
        if (path.ends_with(".geom.glsl")) return shaderc_geometry_shader;
        if (path.ends_with(".tesc.glsl")) return shaderc_tess_control_shader;
        if (path.ends_with(".tese.glsl")) return shaderc_tess_evaluation_shader;
        return shaderc_glsl_infer_from_source;
    }
}

namespace nous::engine::shader_system
{
    bool CompileGlslFileToSpirvFile(const std::string& glslPath,
                                    const std::string& spvPath,
                                    const bool optimize,
                                    const bool debugInfo)
    {
        NOUS_DEBUG_C(CURRENT_CHANNEL, "Compiling '%s' -> '%s'", glslPath.c_str(), spvPath.c_str());

        std::string source;
        if (!ReadTextFile(glslPath, source))
        {
            NOUS_ERROR_C(CURRENT_CHANNEL, "Failed to read GLSL file: %s", glslPath.c_str());
            return false;
        }

        const shaderc::Compiler compiler;
        shaderc::CompileOptions options;

        options.SetTargetEnvironment(shaderc_target_env_vulkan,
                                     shaderc_env_version_vulkan_1_2);
        options.SetTargetSpirv(shaderc_spirv_version_1_5);

        options.SetOptimizationLevel(
                optimize ? shaderc_optimization_level_performance
                         : shaderc_optimization_level_zero);

        if (debugInfo)
            options.SetGenerateDebugInfo();

        const shaderc_shader_kind kind = InferKindFromExtension(glslPath);

        const shaderc::SpvCompilationResult result =
                compiler.CompileGlslToSpv(source,
                                          kind,
                                          glslPath.c_str(),
                                          "main",
                                          options);

        if (result.GetCompilationStatus() != shaderc_compilation_status_success)
        {
            NOUS_ERROR_C(CURRENT_CHANNEL, "Shader compile failed '%s': %s",
                         glslPath.c_str(), result.GetErrorMessage().c_str());
            return false;
        }

        const std::vector spirv(result.cbegin(), result.cend());

        if (!WriteBinaryFile(spvPath,
                             spirv.data(),
                             spirv.size() * sizeof(uint32_t)))
        {
            NOUS_ERROR_C(CURRENT_CHANNEL, "Failed to write SPIR-V file: %s", spvPath.c_str());
            return false;
        }

        NOUS_DEBUG_C(CURRENT_CHANNEL, "Compiled '%s' -> %zu words", glslPath.c_str(), spirv.size());
        return true;
    }

    ShaderCompileResult CompileGlslStringToSpirv(std::string_view glsl, const ShaderStage stage,
    const ShaderCompilerConfig &config, const std::string_view virtualPath)
    {
        ShaderSource source;
        source.virtualPath = virtualPath;
        source.stage = stage;
        source.entryPoint = config.entryPoint;
        source.glslSource.assign(glsl.begin(), glsl.end());

        NOUS_DEBUG_C(CURRENT_CHANNEL, "Compiling stage from '%s'", source.virtualPath.c_str());

        if (glsl.empty())
        {
            NOUS_ERROR_C(CURRENT_CHANNEL, "GLSL source is empty for '%s'", source.virtualPath.c_str());
            return std::unexpected("ShaderCompiler: GLSL source is empty.");
        }

        if (stage == ShaderStage::Unknown)
        {
            NOUS_ERROR_C(CURRENT_CHANNEL, "ShaderStage is Unknown for '%s'", source.virtualPath.c_str());
            return std::unexpected("ShaderCompiler: ShaderStage is Unknown.");
        }

        const shaderc::Compiler compiler;
        shaderc::CompileOptions options;

        options.SetTargetEnvironment(shaderc_target_env_vulkan,
                                     shaderc_env_version_vulkan_1_2);
        options.SetTargetSpirv(shaderc_spirv_version_1_5);

        options.SetOptimizationLevel(ToShadercOpt(config.optimization));

        if (config.generateDebugInfo)
            options.SetGenerateDebugInfo();

        if (config.warningsAsErrors)
            options.SetWarningsAsErrors();

        const shaderc_shader_kind kind = ToShadercKind(stage);

        const shaderc::SpvCompilationResult result =
                compiler.CompileGlslToSpv(
                    source.glslSource,                 // must be std::string
                    kind,
                    source.virtualPath.c_str(),
                    source.entryPoint.c_str(),
                    options
                );

        if (result.GetCompilationStatus() != shaderc_compilation_status_success)
        {
            std::string errorMessage = result.GetErrorMessage();
            NOUS_ERROR_C(CURRENT_CHANNEL, "Compilation failed for '%s': %s",
                         source.virtualPath.c_str(), errorMessage.c_str());
            return std::unexpected(std::move(errorMessage));
        }

        source.spirvBinary.assign(result.cbegin(), result.cend());

        if (source.spirvBinary.empty())
        {
            NOUS_WARN_C(CURRENT_CHANNEL, "SPIR-V output is empty for '%s'", source.virtualPath.c_str());
            return std::unexpected("ShaderCompiler: compilation succeeded but SPIR-V output is empty (unexpected).");
        }

        NOUS_DEBUG_C(CURRENT_CHANNEL, "Stage compiled '%s' -> %zu words",
                     source.virtualPath.c_str(), source.spirvBinary.size());

        return source;
    }
}
