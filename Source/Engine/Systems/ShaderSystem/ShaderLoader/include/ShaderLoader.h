#ifndef NOUS_ENGINE_SHADERLOADER_H
#define NOUS_ENGINE_SHADERLOADER_H

#include "ShaderLoaderTypes.h"

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