#ifndef NOUS_ENGINE_SHADERLOADER_H
#define NOUS_ENGINE_SHADERLOADER_H

#include <ShaderSystem/ShaderLoader/ShaderLoaderTypes.h>
#include <ShaderSystem/ShaderCompiler/ShaderCompilerTypes.h>

// ShaderLoader.h
namespace nous::engine::shader_system
{
    ShaderLoadResult LoadShaderFromFile(const std::string& path,
                                        const ShaderCompilerConfig& config);

    ShaderLoadResult LoadShaderFromSource(const std::string& fullSource,
                                          const std::string& debugName,
                                          const ShaderCompilerConfig& config);
}

#endif //NOUS_ENGINE_SHADERLOADER_H