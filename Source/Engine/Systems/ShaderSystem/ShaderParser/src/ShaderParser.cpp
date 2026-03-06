#include "Engine/Systems/ShaderSystem/ShaderParser/include/ShaderParser.h"

#include "Engine/Systems/ShaderSystem/ShaderTypes.h"

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

NOUS_ShaderSystem::ParseResult NOUS_ShaderSystem::ParseShaderStages(const std::string& fullSource)
{
    ParseResult result;

    const std::string token = "#pragma stage ";
    std::vector<std::pair<ShaderStage, size_t>> stagePositions;

    // Encontrar todas las líneas "#pragma stage <name>"
    size_t pos = 0;
    while ((pos = fullSource.find(token, pos)) != std::string::npos)
    {
        size_t lineEnd = fullSource.find('\n', pos);
        std::string stageName = fullSource.substr(pos + token.size(),
                                                  lineEnd - pos - token.size());

        // Trim whitespace/CR
        stageName.erase(stageName.find_last_not_of(" \t\r\n") + 1);

        ShaderStage stage = ParseStageName(stageName);
        if (stage == ShaderStage::Unknown)
        {
            result.errorMessage = "ParseShaderStages: unknown stage '" + stageName + "'";
            return result;
        }

        // El source de esta stage empieza justo después del \n del pragma
        stagePositions.push_back({ stage, lineEnd + 1 });
        pos = lineEnd;
    }

    if (stagePositions.empty())
    {
        result.errorMessage = "ParseShaderStages: no '#pragma stage' directives found.";
        return result;
    }

    // Extraer el texto de cada stage (de su inicio hasta el siguiente pragma)
    for (size_t i = 0; i < stagePositions.size(); ++i)
    {
        const size_t start = stagePositions[i].second;
        const size_t end   = (i + 1 < stagePositions.size())
                           ? stagePositions[i + 1].second - token.size() - 10 // antes del pragma
                           : fullSource.size();
        // Más robusto: buscar la línea del siguiente pragma hacia atrás
        // (ver nota abajo)

        RawStage raw;
        raw.stage      = stagePositions[i].first;
        raw.glslSource = fullSource.substr(start, end - start);
        result.stages.push_back(std::move(raw));
    }

    result.success = true;
    return result;
}