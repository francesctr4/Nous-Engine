#ifndef NOUS_ENGINE_SHADERLOADER_H
#define NOUS_ENGINE_SHADERLOADER_H

#include "Engine/Systems/ShaderSystem/ShaderLoader/include/ShaderLoaderTypes.h"
#include "Engine/Systems/ShaderSystem/ShaderCompiler/include/ShaderCompilerTypes.h"

// ShaderLoader.h
namespace NOUS_ShaderSystem
{
    ShaderLoadResult LoadShaderFromFile(const std::string& path,
                                        const ShaderCompilerConfig& config);

    ShaderLoadResult LoadShaderFromSource(const std::string& fullSource,
                                          const std::string& debugName,
                                          const ShaderCompilerConfig& config);
}

#endif //NOUS_ENGINE_SHADERLOADER_H