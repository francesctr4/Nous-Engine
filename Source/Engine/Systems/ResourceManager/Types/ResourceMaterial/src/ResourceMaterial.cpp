#include "Engine/Systems/ResourceManager/Types/ResourceMaterial/include/ResourceMaterial.h"
#include "Engine/Systems/ResourceManager/Types/ResourceShader/include/ResourceShader.h"
#include "Engine/Systems/ResourceManager/Types/ResourceTexture/include/ResourceTexture.h"

ResourceMaterial::ResourceMaterial(const uint32 uid) : Resource(uid, ResourceType::MATERIAL)
{
	internalID = INVALID_ID;
	shaderUID  = INVALID_ID;
}

ResourceMaterial::~ResourceMaterial() = default;

void ResourceMaterial::SetShader(ResourceShader* newShader)
{
    shader          = newShader;
    shaderUID       = newShader ? newShader->GetUID() : INVALID_ID;
    poolOwnerShader = nullptr;  // re-derived by VulkanBackend::CreateMaterial
}
