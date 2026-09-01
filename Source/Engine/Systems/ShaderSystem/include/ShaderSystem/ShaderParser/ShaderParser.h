#ifndef NOUS_ENGINE_SHADER_PARSER_H
#define NOUS_ENGINE_SHADER_PARSER_H

#include <expected>
#include <string>
#include <vector>

#include <ShaderSystem/ShaderLoader/ShaderLoaderTypes.h>

namespace nous::engine::shader_system
{
    // Extracts the stages from a unified .glsl file.
    // On failure the error holds a human-readable message.
    using ParseResult = std::expected<std::vector<RawStage>, std::string>;

    ParseResult ParseShaderStages(const std::string& fullSource);
}

#endif //NOUS_ENGINE_SHADER_PARSER_H