#include "Engine/Systems/ResourceManager/Resource/ResourceShader/include/ResourceShader.h"

ResourceShader::ResourceShader(uint32 uid) : Resource(uid, ResourceType::SHADER)
{
    ID = INVALID_ID;
    internalID = INVALID_ID;
    generation = INVALID_ID;
}

ResourceShader::~ResourceShader()
{
}