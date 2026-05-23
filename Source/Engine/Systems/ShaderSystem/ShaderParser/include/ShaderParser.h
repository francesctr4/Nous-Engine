#ifndef NOUS_ENGINE_SHADER_PARSER_H
#define NOUS_ENGINE_SHADER_PARSER_H

#include <string>
#include <vector>

#include "Engine/Systems/ShaderSystem/ShaderLoader/include/ShaderLoaderTypes.h"

namespace nous::engine::shader_system
{
    // Extrae las stages de un archivo .glsl unificado.
    // Devuelve error en result.errorMessage si no encuentra ninguna stage.
    struct ParseResult
    {
        bool success = false;
        std::string errorMessage;
        std::vector<RawStage> stages;
    };

    ParseResult ParseShaderStages(const std::string& fullSource);
}

#endif //NOUS_ENGINE_SHADER_PARSER_H