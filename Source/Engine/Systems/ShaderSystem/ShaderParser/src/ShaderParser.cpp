#include "Engine/Systems/ShaderSystem/ShaderParser/include/ShaderParser.h"
#include <Logger/Logger.h>
#include "Engine/Systems/ShaderSystem/ShaderTypes.h"

constexpr auto CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_SYSTEM_SHADERSYSTEM;

// ShaderParser.cpp
static ShaderStage ParseStageName(const std::string& name)
{
    if (name == "vertex")           return ShaderStage::Vertex;
    if (name == "tessControl")      return ShaderStage::TessControl;
    if (name == "tessEvaluation")   return ShaderStage::TessEvaluation;
    if (name == "geometry")         return ShaderStage::Geometry;
    if (name == "fragment")         return ShaderStage::Fragment;
    if (name == "compute")          return ShaderStage::Compute;

    return ShaderStage::Unknown;
}

nous::engine::shader_system::ParseResult nous::engine::shader_system::ParseShaderStages(const std::string& fullSource)
{
    NOUS_DEBUG_C(CURRENT_CHANNEL, "Parsing shader stages (%zu bytes)", fullSource.size());

    ParseResult result;

    constexpr std::string_view token = "#pragma stage ";

    struct StageEntry
    {
        ShaderStage stage;
        size_t      pragmaStart;  // position of '#pragma stage ...' in fullSource
        size_t      contentStart; // position after the '\n' of that pragma line
    };
    std::vector<StageEntry> stagePositions;

    // Encontrar todas las líneas "#pragma stage <name>"
    size_t pos = 0;
    while ((pos = fullSource.find(token, pos)) != std::string::npos)
    {
        const size_t lineEnd = fullSource.find('\n', pos);
        std::string stageName = fullSource.substr(pos + token.size(),
                                                  lineEnd - pos - token.size());

        // Trim whitespace/CR
        stageName.erase(stageName.find_last_not_of(" \t\r\n") + 1);

        const ShaderStage stage = ParseStageName(stageName);
        if (stage == ShaderStage::Unknown)
        {
            result.errorMessage = "ParseShaderStages: unknown stage '" + stageName + "'";
            NOUS_ERROR_C(CURRENT_CHANNEL, "Unknown stage directive: '%s'", stageName.c_str());
            return result;
        }

        stagePositions.push_back({ stage, pos, lineEnd + 1 });
        pos = lineEnd;
    }

    if (stagePositions.empty())
    {
        result.errorMessage = "ParseShaderStages: no '#pragma stage' directives found.";
        NOUS_ERROR_C(CURRENT_CHANNEL, "No '#pragma stage' directives found in shader source");
        return result;
    }

    // Extraer el texto de cada stage (de su inicio hasta el siguiente pragma)
    for (size_t i = 0; i < stagePositions.size(); ++i)
    {
        const size_t start = stagePositions[i].contentStart;
        const size_t end   = i + 1 < stagePositions.size()
                           ? stagePositions[i + 1].pragmaStart
                           : fullSource.size();

        RawStage raw;
        raw.stage      = stagePositions[i].stage;
        raw.glslSource = fullSource.substr(start, end - start);
        result.stages.push_back(std::move(raw));
    }

    result.success = true;
    NOUS_DEBUG_C(CURRENT_CHANNEL, "Parsed %zu stage(s)", result.stages.size());
    return result;
}