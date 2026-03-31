#include "Engine/Systems/ResourceManager/Importer/ImporterMaterial/include/ImporterMaterial.h"

#include "Engine/Systems/ResourceManager/Resource/ResourceMaterial/include/ResourceMaterial.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceTexture/include/ResourceTexture.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceShader/include/ResourceShader.h"
#include "Engine/Modules/ModuleResourceManager/include/ModuleResourceManager.h"
#include "Engine/Renderer/IGPUResourceFactory.h"

#include "Engine/Core/FileSystem/FileSystem.h"
#include "Engine/Utils/Serialization/JsonFile/JsonFile.h"
#include "Engine/Systems/ResourceManager/Resource/MetaFileData.inl"
#include <parson.h>
#include <algorithm>

#include "Engine/Core/MemoryManager/MemoryManager.h"

#include "Engine/Core/Logger/Logger.h"

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
            if (mResourceManager->GetAssetMetaData(texPath, texMeta))
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

bool ImporterMaterial::Deserialize(const std::string& libraryPath, Resource* outResource)
{
    ResourceMaterial* material = down_cast<ResourceMaterial*>(outResource);

    JsonFile jsonFile;
    if (!jsonFile.LoadFromFile(libraryPath.c_str()))
    {
        NOUS_ERROR("ImporterMaterial::Deserialize() failed to load file '%s'", libraryPath.c_str());
        return false;
    }

    if (!jsonFile.GetValue("diffuse_color", material->diffuseColor))
    {
        NOUS_ERROR("ImporterMaterial::Deserialize() missing diffuse_color in '%s'", libraryPath.c_str());
        return false;
    }

    std::string diffuseMapPath;
    if (!jsonFile.GetValue("diffuse_map_path", diffuseMapPath))
    {
        NOUS_ERROR("ImporterMaterial::Deserialize() missing diffuse_map_path in '%s'", libraryPath.c_str());
        return false;
    }

    // Resolve diffuse texture dependency (CPU side — may push texture to upload queue).
    // Prefer library-only path (GAME mode / no Assets/).
    ResourceTexture* diffuseTexture = nullptr;
    double texUIDDouble = 0.0;
    std::string texLibPath;
    if (jsonFile.GetValue("diffuse_map_uid", texUIDDouble) &&
        jsonFile.GetValue("diffuse_map_library_path", texLibPath))
    {
        const UID texUID = static_cast<UID>(texUIDDouble);
        const std::string texName = NOUS_FileManager::GetFilename(diffuseMapPath);
        diffuseTexture = down_cast<ResourceTexture*>(
            mResourceManager->CreateResourceFromLibrary(
                texUID, ResourceType::TEXTURE, texName, diffuseMapPath, texLibPath));
    }
    if (!diffuseTexture)
    {
        diffuseTexture = down_cast<ResourceTexture*>(
            mResourceManager->CreateResource(diffuseMapPath));
    }

    material->diffuseMap.type    = TextureMapType::DIFFUSE;
    material->diffuseMap.texture = diffuseTexture;

    // Resolve optional shader dependency.
    double shaderUIDDouble = 0.0;
    std::string shaderAssetPath, shaderLibPath;
    if (jsonFile.GetValue("shader_uid",       shaderUIDDouble) &&
        jsonFile.GetValue("shader_asset_path", shaderAssetPath))
    {
        const UID         shaderUID  = static_cast<UID>(shaderUIDDouble);
        const std::string shaderName = NOUS_FileManager::GetFilename(shaderAssetPath);
        ResourceShader*   loadedShader = nullptr;

        // Prefer library path (GAME mode / no Assets/).
        if (jsonFile.GetValue("shader_library_path", shaderLibPath))
        {
            Resource* r = mResourceManager->CreateResourceFromLibrary(
                shaderUID, ResourceType::SHADER, shaderName,
                shaderAssetPath, shaderLibPath);
            if (r) loadedShader = down_cast<ResourceShader*>(r);
        }
        if (!loadedShader)
        {
            Resource* r = mResourceManager->CreateResource(shaderAssetPath);
            if (r) loadedShader = down_cast<ResourceShader*>(r);
        }

        if (loadedShader)
        {
            material->shader    = loadedShader;
            material->shaderUID = loadedShader->GetUID();
        }
        else
        {
            NOUS_WARN("ImporterMaterial::Deserialize() — shader '%s' could not be loaded; falling back to built-in.",
                      shaderAssetPath.c_str());
        }
    }

    return true;
}

bool ImporterMaterial::Upload(Resource* outResource, IGPUResourceFactory* gpu)
{
    ResourceMaterial* material = down_cast<ResourceMaterial*>(outResource);
    return gpu->CreateMaterial(material);
}

void ImporterMaterial::Release(Resource* inResource, IGPUResourceFactory* gpu)
{
    ResourceMaterial* material = down_cast<ResourceMaterial*>(inResource);
    // Guard: the resource may never have been GPU-uploaded (e.g. scene cleared
    // before the pending upload was processed). Only release if it has a valid slot.
    if (material->internalID != INVALID_ID)
        gpu->DestroyMaterial(material);
}

void ImporterMaterial::Evict(Resource* inResource)
{
    ResourceMaterial* material = down_cast<ResourceMaterial*>(inResource);
    if (material->diffuseMap.texture)
    {
        mResourceManager->UnloadResource(material->diffuseMap.texture->GetUID());
        material->diffuseMap.texture = nullptr;
    }
    if (material->shader)
    {
        mResourceManager->UnloadResource(material->shader->GetUID());
        material->shader    = nullptr;
        material->shaderUID = INVALID_ID;
    }
}

bool ImporterMaterial::SaveMaterialToAssets(ResourceMaterial* material)
{
    if (!material) return false;

    // Read a .nmat JSON file, update shader keys, and write it back.
    auto updateFile = [&](const std::string& filePath) -> bool
    {
        JSON_Value* val = json_parse_file(filePath.c_str());
        if (!val)
        {
            NOUS_ERROR("ImporterMaterial::SaveMaterialToAssets() — could not open '%s'", filePath.c_str());
            return false;
        }
        JSON_Object* obj = json_value_get_object(val);

        if (material->shader)
        {
            json_object_set_number(obj, "shader_uid",
                static_cast<double>(material->shaderUID));
            json_object_set_string(obj, "shader_asset_path",
                material->shader->GetAssetsPath().c_str());
            json_object_set_string(obj, "shader_library_path",
                material->shader->GetLibraryPath().c_str());
        }
        else
        {
            json_object_remove(obj, "shader_uid");
            json_object_remove(obj, "shader_asset_path");
            json_object_remove(obj, "shader_library_path");
        }

        const bool ok = (json_serialize_to_file_pretty(val, filePath.c_str()) == JSONSuccess);
        json_value_free(val);
        return ok;
    };

    bool ok = updateFile(material->GetAssetsPath());
    ok     &= updateFile(material->GetLibraryPath());
    return ok;
}
