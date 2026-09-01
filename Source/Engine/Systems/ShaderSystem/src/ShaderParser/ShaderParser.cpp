#include <ShaderSystem/ShaderParser/ShaderParser.h>
#include <Logger/Logger.h>
#include <ShaderSystem/ShaderTypes.h>
#include <cstddef>

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

    std::vector<RawStage> stages;

    constexpr std::string_view token = "#pragma stage ";

    struct StageEntry
    {
        ShaderStage stage;
        size_t      pragmaStart;  // position of '#pragma stage ...' in fullSource
        size_t      contentStart; // position after the '\n' of that pragma line
    };
    std::vector<StageEntry> stagePositions;

    // Find all "#pragma stage <name>" lines
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
            NOUS_ERROR_C(CURRENT_CHANNEL, "Unknown stage directive: '%s'", stageName.c_str());
            return std::unexpected("ParseShaderStages: unknown stage '" + stageName + "'");
        }

        stagePositions.push_back({ stage, pos, lineEnd + 1 });
        pos = lineEnd;
    }

    if (stagePositions.empty())
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "No '#pragma stage' directives found in shader source");
        return std::unexpected("ParseShaderStages: no '#pragma stage' directives found.");
    }

    // Extract the text of each stage (from its start to the next pragma)
    for (size_t i = 0; i < stagePositions.size(); ++i)
    {
        const size_t start = stagePositions[i].contentStart;
        const size_t end   = i + 1 < stagePositions.size()
                           ? stagePositions[i + 1].pragmaStart
                           : fullSource.size();

        RawStage raw;
        raw.stage      = stagePositions[i].stage;
        raw.glslSource = fullSource.substr(start, end - start);
        stages.push_back(std::move(raw));
    }

    NOUS_DEBUG_C(CURRENT_CHANNEL, "Parsed %zu stage(s)", stages.size());
    return stages;
}