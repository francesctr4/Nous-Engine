#include "Engine/Systems/ResourceManager/ResourceTypes/ResourceShader/include/ResourceShader.h"

ResourceShader::ResourceShader(uint32 uid) : Resource(uid, ResourceType::SHADER)
{
    generation = INVALID_ID;
}

ResourceShader::~ResourceShader()
{
}