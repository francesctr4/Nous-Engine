#ifndef NOUS_ENGINE_SHADERLOADERTYPES_H
#define NOUS_ENGINE_SHADERLOADERTYPES_H

#include "Engine/Systems/ShaderSystem/ShaderTypes.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceShader/include/ResourceShader.h"

struct RawStage
{
    ShaderStage stage;
    std::string glslSource; // texto de esa stage, con su #version etc.
};

struct ShaderLoadResult
{
    bool        success = false;
    std::string errorMessage;
    std::string sourcePath;

    ResourceShader* shader = nullptr; // owned, tú lo gestiónas con tu allocator
};

#endif //NOUS_ENGINE_SHADERLOADERTYPES_H