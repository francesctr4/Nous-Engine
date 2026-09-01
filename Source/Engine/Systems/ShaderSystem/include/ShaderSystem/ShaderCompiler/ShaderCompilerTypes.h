#ifndef NOUS_ENGINE_SHADER_COMPILER_TYPES_H
#define NOUS_ENGINE_SHADER_COMPILER_TYPES_H

#include <cstdint>
#include <expected>
#include <string>

#include <ShaderSystem/ShaderTypes.h>

enum class ShaderOptimizationLevel : std::int8_t
{
    Unknown = -1,

    Zero = 0,
    Size = 1,
    Performance = 2,

    ALL
};

struct ShaderCompilerConfig
{
    ShaderOptimizationLevel optimization = ShaderOptimizationLevel::Performance;
    bool generateDebugInfo = true;
    bool warningsAsErrors = false;
    std::string entryPoint = "main";
};

using ShaderCompileResult = std::expected<ShaderSource, std::string>;

#endif //NOUS_ENGINE_SHADER_COMPILER_TYPES_H