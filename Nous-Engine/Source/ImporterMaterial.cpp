#include "ImporterMaterial.h"

#include "ResourceMaterial.h"
#include "ResourceTexture.h"

#include "ModuleResourceManager.h"

#include "FileManager.h"
#include "JsonFile.h"
#include "MetaFileData.inl"

#include "MemoryManager.h"

bool ImporterMaterial::Import(const MetaFileData& metaFileData)
{
    Resource* tempMaterial = NOUS_NEW<ResourceMaterial>(MemoryManager::MemoryTag::RESOURCE_MATERIAL);
    return Save(metaFileData, tempMaterial);
}

bool ImporterMaterial::Save(const MetaFileData& metaFileData, Resource*& inResource)
{
	ResourceMaterial* material = static_cast<ResourceMaterial*>(inResource);
	if (!material) return false;
    NOUS_DELETE<ResourceMaterial>(material, MemoryManager::MemoryTag::RESOURCE_MATERIAL);

    bool ret = NOUS_FileManager::CopyFile(metaFileData.assetsPath, metaFileData.libraryPath);

    return ret;
}

bool ImporterMaterial::Load(const MetaFileData& metaFileData, Resource* outResource)
{
    ResourceMaterial* material = static_cast<ResourceMaterial*>(outResource);
    if (!material) return false;

    JsonFile jsonFile;

    if (!jsonFile.LoadFromFile(metaFileData.libraryPath.c_str()))
    {
        NOUS_ERROR("Error in ImporterMaterial::Load(). Unable to load the file");
        return false;
    }

    if (!jsonFile.GetValue("diffuse_color", material->diffuseColor))
    {
        NOUS_ERROR("Error in ImporterMaterial::Load(). Missing or invalid diffuse_color field");
        return false;
    }

    std::string diffuseMapPath;
    if (!jsonFile.GetValue("diffuse_map_path", diffuseMapPath))
    {
        NOUS_ERROR("Error in ImporterMaterial::Load(). Missing or invalid diffuse_map_path field");
        return false;
    }

    // Diffuse Texture
    ResourceTexture* diffuseTexture = static_cast<ResourceTexture*>(External->resourceManager->CreateResource(diffuseMapPath));
    material->diffuseMap.type = TextureMapType2::DIFFUSE;
    material->diffuseMap.texture = diffuseTexture;

    return true;
}

bool ImporterMaterial::Unload(Resource*& inResource)
{
	ResourceMaterial* material = static_cast<ResourceMaterial*>(inResource);
	if (!material) return false;

    External->resourceManager->UnloadResource(material->diffuseMap.texture->GetUID());

	return true;
}
