#include <ResourceManager/Types/ResourceMaterial/ResourceMaterial.h>
#include <ResourceManager/Types/ResourceShader/ResourceShader.h>
#include <ResourceManager/Types/ResourceTexture/ResourceTexture.h>

ResourceMaterial::ResourceMaterial(const uint32 uid) : ResourceBase(uid, ResourceType::MATERIAL)
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
