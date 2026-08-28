#include <ResourceManager/Types/ResourceShader/ResourceShader.h>
#include <EngineCore/InvalidID.h>

ResourceShader::ResourceShader(uint32_t uid) : ResourceBase(uid, ResourceType::SHADER)
{
    generation = INVALID_ID;
}

ResourceShader::~ResourceShader()
{
}