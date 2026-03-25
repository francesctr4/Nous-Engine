#include "Engine/Systems/ResourceManager/Importer/ImporterMaterial/include/ImporterMaterial.h"

#include "Engine/Systems/ResourceManager/Resource/ResourceMaterial/include/ResourceMaterial.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceTexture/include/ResourceTexture.h"
#include "Engine/Core/Application.h"
#include "Engine/Modules/ModuleResourceManager/include/ModuleResourceManager.h"

#include "Engine/Core/FileSystem/FileSystem.h"
#include "Engine/Utils/Serialization/JsonFile/JsonFile.h"
#include "Engine/Systems/ResourceManager/Resource/MetaFileData.inl"
#include <parson.h>
#include <algorithm>

#include "Engine/Core/MemoryManager/MemoryManager.h"

#include "Engine/Modules/ModuleRenderer3D/include/ModuleRenderer3D.h"
#include "Engine/Core/Logger/Logger.h"
#include "Engine/Renderer/Frontend/RendererFrontend.h"

bool ImporterMaterial::Import(const MetaFileData& metaFileData)
{
    Resource* tempMaterial = NOUS_NEW<ResourceMaterial>(MemoryTag::RESOURCE_MATERIAL);
    return Save(metaFileData, tempMaterial);
}

bool ImporterMaterial::Save(const MetaFileData& metaFileData, Resource*& inResource)
{
    NOUS_DELETE(inResource, MemoryTag::RESOURCE_MATERIAL);

    // Try to enrich the Library .nmat with the texture UID so GAME mode can load
    // textures from Library without needing any .meta sidecar files.
    JSON_Value* srcVal = json_parse_file(metaFileData.assetsPath.c_str());
    if (srcVal)
    {
        JSON_Object* srcObj = json_value_get_object(srcVal);
        const char* rawPath = json_object_get_string(srcObj, "diffuse_map_path");
        if (rawPath)
        {
            std::string texPath(rawPath);
            std::replace(texPath.begin(), texPath.end(), '\\', '/');

            MetaFileData texMeta;
            if (External && External->resourceManager &&
                External->resourceManager->GetAssetMetaData(texPath, texMeta))
            {
                // Normalize library path to forward slashes before storing.
                std::string libPath = texMeta.libraryPath;
                std::replace(libPath.begin(), libPath.end(), '\\', '/');

                json_object_set_number(srcObj, "diffuse_map_uid",          static_cast<double>(texMeta.uid));
                json_object_set_string(srcObj, "diffuse_map_library_path", libPath.c_str());
            }
        }
        bool ret = json_serialize_to_file_pretty(srcVal, metaFileData.libraryPath.c_str()) == JSONSuccess;
        json_value_free(srcVal);
        return ret;
    }

    return NOUS_FileManager::CopyFile(metaFileData.assetsPath, metaFileData.libraryPath);
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

    // Diffuse Texture — prefer library-only path (GAME mode / no Assets/).
    ResourceTexture* diffuseTexture = nullptr;
    double texUIDDouble = 0.0;
    std::string texLibPath;
    if (jsonFile.GetValue("diffuse_map_uid", texUIDDouble) &&
        jsonFile.GetValue("diffuse_map_library_path", texLibPath))
    {
        const UID texUID = static_cast<UID>(texUIDDouble);
        const std::string texName = NOUS_FileManager::GetFilename(diffuseMapPath);
        diffuseTexture = down_cast<ResourceTexture*>(
            External->resourceManager->CreateResourceFromLibrary(
                texUID, ResourceType::TEXTURE, texName, diffuseMapPath, texLibPath));
    }
    if (!diffuseTexture)
    {
        diffuseTexture = down_cast<ResourceTexture*>(
            External->resourceManager->CreateResource(diffuseMapPath));
    }

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
