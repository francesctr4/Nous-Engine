#include "Engine/Systems/ResourceManager/Types/ResourceShader/include/ResourceShader.h"

ResourceShader::ResourceShader(uint32 uid) : ResourceBase(uid, ResourceType::SHADER)
{
    generation = INVALID_ID;
}

ResourceShader::~ResourceShader()
{
}