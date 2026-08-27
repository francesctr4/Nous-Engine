#ifndef NOUS_ENGINE_SHADERCOMPILATOR_H
#define NOUS_ENGINE_SHADERCOMPILATOR_H

#pragma once

#include <string>

#include <ShaderSystem/ShaderCompiler/ShaderCompilerTypes.h>
#include <ShaderSystem/ShaderTypes.h>

namespace nous::engine::shader_system
{
    /**
     * Compiles a GLSL file into a SPIR-V binary file.
     *
     * @param glslPath   Path to the .glsl source file
     * @param spvPath    Output path for the .spv file
     * @param optimize   Enable SPIR-V optimization
     * @param debugInfo  Generate debug information
     * @return true on success, false on failure
     */
    bool CompileGlslFileToSpirvFile(const std::string& glslPath,
                                    const std::string& spvPath,
                                    bool optimize = true,
                                    bool debugInfo = true);

    ShaderCompileResult CompileGlslStringToSpirv(std::string_view glsl,
                                                 ShaderStage stage,
                                                 const ShaderCompilerConfig& config,
                                                 std::string_view virtualPath);
}

#endif //NOUS_ENGINE_SHADERCOMPILATOR_H
