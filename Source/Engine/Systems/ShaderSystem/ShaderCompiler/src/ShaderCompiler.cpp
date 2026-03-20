#include "Engine/Systems/ShaderSystem/ShaderCompiler/include/ShaderCompiler.h"
#include "Engine/Core/Logger/Logger.h"

constexpr LogChannel CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_SYSTEM_SHADERSYSTEM;

#include <shaderc/shaderc.hpp>

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace
{
    bool ReadTextFile(const std::string& path, std::string& out)
    {
        std::ifstream f(path, std::ios::binary);
        if (!f) return false;

        out.assign(std::istreambuf_iterator<char>(f),
                   std::istreambuf_iterator<char>());
        return true;
    }

    bool WriteBinaryFile(const std::string& path, const void* data, size_t size)
    {
        std::ofstream f(path, std::ios::binary);
        if (!f) return false;

        f.write(reinterpret_cast<const char*>(data),
                static_cast<std::streamsize>(size));
        return f.good();
    }

    shaderc_shader_kind ToShadercKind(ShaderStage stage)
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

    shaderc_optimization_level ToShadercOpt(ShaderOptimizationLevel opt)
    {
        switch (opt)
        {
            case ShaderOptimizationLevel::Zero:        return shaderc_optimization_level_zero;
            case ShaderOptimizationLevel::Size:        return shaderc_optimization_level_size;
            case ShaderOptimizationLevel::Performance: return shaderc_optimization_level_performance;
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

namespace NOUS_ShaderSystem
{
    bool CompileGlslFileToSpirvFile(const std::string& glslPath,
                                    const std::string& spvPath,
                                    bool optimize,
                                    bool debugInfo)
    {
        NOUS_TRACE_C(CURRENT_CHANNEL, "Compiling '%s' -> '%s'", glslPath.c_str(), spvPath.c_str());

        std::string source;
        if (!ReadTextFile(glslPath, source))
        {
            NOUS_ERROR_C(CURRENT_CHANNEL, "Failed to read GLSL file: %s", glslPath.c_str());
            return false;
        }

        shaderc::Compiler compiler;
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

        shaderc::SpvCompilationResult result =
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

        std::vector<uint32_t> spirv(result.cbegin(), result.cend());

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

    ShaderCompileResult CompileGlslStringToSpirv(std::string_view glsl, ShaderStage stage,
    const ShaderCompilerConfig &config, std::string_view virtualPath)
    {
        ShaderCompileResult out{};
        out.success = false;

        // Build ShaderSource inside the result (as you requested)
        out.shaderSource.virtualPath = virtualPath;
        out.shaderSource.stage = stage;
        out.shaderSource.entryPoint = config.entryPoint;
        out.shaderSource.glslSource.assign(glsl.begin(), glsl.end());

        NOUS_TRACE_C(CURRENT_CHANNEL, "Compiling stage from '%s'", out.shaderSource.virtualPath.c_str());

        if (glsl.empty())
        {
            NOUS_ERROR_C(CURRENT_CHANNEL, "GLSL source is empty for '%s'", out.shaderSource.virtualPath.c_str());
            out.errorMessage = "ShaderCompiler: GLSL source is empty.";
            return out;
        }

        if (stage == ShaderStage::Unknown)
        {
            NOUS_ERROR_C(CURRENT_CHANNEL, "ShaderStage is Unknown for '%s'", out.shaderSource.virtualPath.c_str());
            out.errorMessage = "ShaderCompiler: ShaderStage is Unknown.";
            return out;
        }

        shaderc::Compiler compiler;
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

        shaderc::SpvCompilationResult result =
                compiler.CompileGlslToSpv(
                    out.shaderSource.glslSource,                 // must be std::string
                    kind,
                    out.shaderSource.virtualPath.c_str(),
                    out.shaderSource.entryPoint.c_str(),
                    options
                );

        out.errorMessage = result.GetErrorMessage();

        if (result.GetCompilationStatus() != shaderc_compilation_status_success)
        {
            NOUS_ERROR_C(CURRENT_CHANNEL, "Compilation failed for '%s': %s",
                         out.shaderSource.virtualPath.c_str(), out.errorMessage.c_str());
            out.success = false;
            return out;
        }

        out.shaderSource.spirvBinary.assign(result.cbegin(), result.cend());
        out.success = !out.shaderSource.spirvBinary.empty();

        if (!out.success && out.errorMessage.empty())
        {
            out.errorMessage = "ShaderCompiler: compilation succeeded but SPIR-V output is empty (unexpected).";
            NOUS_WARN_C(CURRENT_CHANNEL, "SPIR-V output is empty for '%s'", out.shaderSource.virtualPath.c_str());
        }
        else
        {
            NOUS_DEBUG_C(CURRENT_CHANNEL, "Stage compiled '%s' -> %zu words",
                         out.shaderSource.virtualPath.c_str(), out.shaderSource.spirvBinary.size());
        }

        return out;
    }
}
