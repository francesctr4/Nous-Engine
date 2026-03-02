#ifndef NOUS_ENGINE_SHADER_TYPES_H
#define NOUS_ENGINE_SHADER_TYPES_H

#include <string>
#include <vector>
#include <cstdint>

enum class ShaderStage : std::int8_t
{
    Unknown = -1,

    Vertex = 0,
    TessControl = 1,
    TessEvaluation = 2,
    Geometry = 3,
    Fragment = 4,
    Compute = 5,

    ALL
};

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

class ShaderSource
{
public:

    ShaderStage stage;
    std::string entryPoint = "main";
    std::string glslSource;
    std::vector<uint32_t> spirvBinary;
    std::string virtualPath;
    uint64_t sourceHash = 0;

    bool IsValid() const { return !glslSource.empty() && stage != ShaderStage::Unknown; }
};

struct ShaderCompileResult
{
    bool success;
    std::string errorMessage;

    ShaderSource shaderSource;
};

#endif //NOUS_ENGINE_SHADER_TYPES_H