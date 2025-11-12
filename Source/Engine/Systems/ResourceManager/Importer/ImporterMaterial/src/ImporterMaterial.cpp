#include "Engine/Systems/ResourceManager/Importer/ImporterMaterial/include/ImporterMaterial.h"

#include "Engine/Systems/ResourceManager/Resource/ResourceMaterial/include/ResourceMaterial.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceTexture/include/ResourceTexture.h"
#include "Engine/Core/Application.h"
#include "Engine/Modules/ModuleResourceManager/include/ModuleResourceManager.h"

#include "Engine/Core/FileSystem/FileSystem.h"
#include "Engine/Utils/Serialization/JsonFile/JsonFile.h"
#include "Engine/Systems/ResourceManager/Resource/MetaFileData.inl"

#include "Engine/Core/MemoryManager/MemoryManager.h"

#include "Engine/Modules/ModuleRenderer3D/include/ModuleRenderer3D.h"
#include "Engine/Core/Logger/Logger.h"
#include "Engine/Renderer/Frontend/RendererFrontend.h"

bool ImporterMaterial::Import(const MetaFileData& metaFileData)
{
    Resource* tempMaterial = NOUS_NEW<ResourceMaterial>(MemoryManager::MemoryTag::RESOURCE_MATERIAL);
    return Save(metaFileData, tempMaterial);
}

bool ImporterMaterial::Save(const MetaFileData& metaFileData, Resource*& inResource)
{
    NOUS_DELETE(inResource, MemoryManager::MemoryTag::RESOURCE_MATERIAL);

    bool ret = NOUS_FileManager::CopyFile(metaFileData.assetsPath, metaFileData.libraryPath);

    return ret;
}

bool ImporterMaterial::Load(const std::string& libraryPath, Resource* outResource)
{
    ResourceMaterial* material = down_cast<ResourceMaterial*>(outResource);

    JsonFile jsonFile;

    if (!jsonFile.LoadFromFile(libraryPath.c_str()))
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

    bool ret = true;

    // Diffuse Texture
    ResourceTexture* diffuseTexture = down_cast<ResourceTexture*>(External->resourceManager->CreateResource(diffuseMapPath));
    material->diffuseMap.type = TextureMapType::DIFFUSE;
    material->diffuseMap.texture = diffuseTexture;

    ret = External->renderer->GetRendererFrontend()->CreateMaterial(material);

    return ret;
}

bool ImporterMaterial::Unload(Resource* inResource)
{
	ResourceMaterial* material = down_cast<ResourceMaterial*>(inResource);

    if (material->diffuseMap.texture != nullptr)
    {
        External->resourceManager->UnloadResource(material->diffuseMap.texture->GetUID());
        material->diffuseMap.texture = nullptr;
    }

    External->renderer->GetRendererFrontend()->DestroyMaterial(material);

	return true;
}
