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

#include <cstring>

static const char* UniformValueTypeToString(UniformValueType type)
{
    switch (type)
    {
        case UniformValueType::Float: return "float";
        case UniformValueType::Vec2:  return "vec2";
        case UniformValueType::Vec3:  return "vec3";
        case UniformValueType::Vec4:  return "vec4";
        case UniformValueType::Int:   return "int";
        case UniformValueType::IVec2: return "ivec2";
        case UniformValueType::IVec3: return "ivec3";
        case UniformValueType::IVec4: return "ivec4";
    }
    return "vec4";
}

static UniformValueType StringToUniformValueType(const char* str)
{
    if (!str) return UniformValueType::Vec4;
    if (std::strcmp(str, "float") == 0) return UniformValueType::Float;
    if (std::strcmp(str, "vec2")  == 0) return UniformValueType::Vec2;
    if (std::strcmp(str, "vec3")  == 0) return UniformValueType::Vec3;
    if (std::strcmp(str, "vec4")  == 0) return UniformValueType::Vec4;
    if (std::strcmp(str, "int")   == 0) return UniformValueType::Int;
    if (std::strcmp(str, "ivec2") == 0) return UniformValueType::IVec2;
    if (std::strcmp(str, "ivec3") == 0) return UniformValueType::IVec3;
    if (std::strcmp(str, "ivec4") == 0) return UniformValueType::IVec4;
    return UniformValueType::Vec4;
}

static uint32_t UniformValueComponentCountLocal(UniformValueType type)
{
    switch (type)
    {
        case UniformValueType::Float: case UniformValueType::Int:   return 1;
        case UniformValueType::Vec2:  case UniformValueType::IVec2: return 2;
        case UniformValueType::Vec3:  case UniformValueType::IVec3: return 3;
        case UniformValueType::Vec4:  case UniformValueType::IVec4: return 4;
    }
    return 4;
}

bool ImporterMaterial::Import(const MetaFileData& metaFileData)
{
    Resource* tempMaterial = NOUS_NEW<ResourceMaterial>(MemoryTag::RESOURCE_MATERIAL);
    return Save(metaFileData, tempMaterial);
}

bool ImporterMaterial::Save(const MetaFileData& metaFileData, Resource*& inResource)
{
    NOUS_DELETE(inResource, MemoryTag::RESOURCE_MATERIAL);

    JSON_Value* srcVal = json_parse_file(metaFileData.assetsPath.c_str());
    if (!srcVal)
        return NOUS_FileManager::CopyFile(metaFileData.assetsPath, metaFileData.libraryPath);

    JSON_Object* srcObj = json_value_get_object(srcVal);

    // ── Enrich each entry in the texture_maps array with uid + library_path ──
    if (JSON_Array* texMapsArr = json_object_get_array(srcObj, "texture_maps"))
    {
        const size_t count = json_array_get_count(texMapsArr);
        for (size_t i = 0; i < count; ++i)
        {
            JSON_Object* entry = json_array_get_object(texMapsArr, i);
            if (!entry) continue;
            const char* rawPath = json_object_get_string(entry, "asset_path");
            if (!rawPath) continue;

            std::string texPath = NOUS_FileManager::NormalizePath(rawPath);

            MetaFileData texMeta;
            if (mResourceManager->GetAssetMetaData(texPath, texMeta))
            {
                std::string libPath = NOUS_FileManager::NormalizePath(texMeta.libraryPath);
                json_object_set_number(entry, "uid",          static_cast<double>(texMeta.uid));
                json_object_set_string(entry, "library_path", libPath.c_str());
            }
        }
    }

    // ── Enrich shader_asset_path with shader_uid + shader_library_path ──
    if (const char* rawShaderPath = json_object_get_string(srcObj, "shader_asset_path"))
    {
        std::string shaderPath = NOUS_FileManager::NormalizePath(rawShaderPath);

        MetaFileData shaderMeta;
        if (mResourceManager->GetAssetMetaData(shaderPath, shaderMeta))
        {
            std::string libPath = NOUS_FileManager::NormalizePath(shaderMeta.libraryPath);
            json_object_set_number(srcObj, "shader_uid",          static_cast<double>(shaderMeta.uid));
            json_object_set_string(srcObj, "shader_library_path", libPath.c_str());
        }
    }

    const bool ret = json_serialize_to_file_pretty(srcVal, metaFileData.libraryPath.c_str()) == JSONSuccess;
    json_value_free(srcVal);
    return ret;
}

bool ImporterMaterial::Deserialize(const std::string& libraryPath, Resource* outResource)
{
    ResourceMaterial* material = down_cast<ResourceMaterial*>(outResource);

    JSON_Value* rootVal = json_parse_file(libraryPath.c_str());
    if (!rootVal)
    {
        NOUS_ERROR("ImporterMaterial::Deserialize() failed to load file '%s'", libraryPath.c_str());
        return false;
    }
    JSON_Object* root = json_value_get_object(rootVal);

    // ── Uniforms ─────────────────────────────────────────────────────────────
    // Data-driven: every uniform must live in the "uniforms" array. Members the
    // material doesn't declare are filled with reflection defaults at draw time.
    if (JSON_Array* uniformsArr = json_object_get_array(root, "uniforms"))
    {
        const size_t count = json_array_get_count(uniformsArr);
        for (size_t i = 0; i < count; ++i)
        {
            JSON_Object* entry = json_array_get_object(uniformsArr, i);
            if (!entry) continue;

            const char* name    = json_object_get_string(entry, "name");
            const char* typeStr = json_object_get_string(entry, "type");
            JSON_Array* valArr  = json_object_get_array(entry, "value");
            if (!name || !valArr) continue;

            UniformValue uv;
            uv.type = StringToUniformValueType(typeStr);
            uv.data = glm::vec4(1.0f);

            const uint32_t compCount = UniformValueComponentCountLocal(uv.type);
            const size_t   arrCount  = json_array_get_count(valArr);
            for (uint32_t c = 0; c < compCount && c < arrCount; ++c)
                uv.data[static_cast<int>(c)] = static_cast<float>(json_array_get_number(valArr, c));

            material->uniformValues[name] = uv;
        }
    }

    // ── Texture maps ─────────────────────────────────────────────────────────
    // Data-driven: every texture slot lives in the "texture_maps" array with
    // name + asset_path + uid + library_path. Save() enriches uid/library_path
    // at import time, so library copies always carry them.
    if (JSON_Array* texMapsArr = json_object_get_array(root, "texture_maps"))
    {
        const size_t count = json_array_get_count(texMapsArr);
        for (size_t i = 0; i < count; ++i)
        {
            JSON_Object* entry = json_array_get_object(texMapsArr, i);
            if (!entry) continue;

            const char* name    = json_object_get_string(entry, "name");
            const char* rawPath = json_object_get_string(entry, "asset_path");
            const char* libRaw  = json_object_get_string(entry, "library_path");
            const double uidDouble = json_object_get_number(entry, "uid");
            if (!name || !rawPath || !libRaw || uidDouble == 0.0)
            {
                NOUS_WARN("ImporterMaterial::Deserialize() — texture entry missing name/asset_path/uid/library_path; skipping.");
                continue;
            }

            std::string assetPath = NOUS_FileManager::NormalizePath(rawPath);
            std::string libPath   = NOUS_FileManager::NormalizePath(libRaw);

            const uint32         texUID  = static_cast<uint32>(uidDouble);
            const std::string texName = NOUS_FileManager::GetFilename(assetPath);
            ResourceTexture*  tex     = down_cast<ResourceTexture*>(
                mResourceManager->CreateResourceFromLibrary(
                    texUID, ResourceType::TEXTURE, texName, assetPath, libPath));

            if (tex)
                material->textureMaps[name].texture = tex;
            else
                NOUS_WARN("ImporterMaterial::Deserialize() — texture '%s' (slot '%s') could not be loaded.",
                          assetPath.c_str(), name);
        }
    }

    // ── Shader (optional) ────────────────────────────────────────────────────
    // Data-driven: shader_asset_path + shader_uid + shader_library_path are all
    // required. Save() enriches uid/library_path at import time.
    if (const char* shaderAssetRaw = json_object_get_string(root, "shader_asset_path"))
    {
        const char*  shaderLibRaw = json_object_get_string(root, "shader_library_path");
        const double shaderUID    = json_object_get_number(root, "shader_uid");
        if (!shaderLibRaw || shaderUID == 0.0)
        {
            NOUS_WARN("ImporterMaterial::Deserialize() — shader entry missing shader_uid/shader_library_path; falling back to built-in.");
        }
        else
        {
            std::string       shaderAssetPath(shaderAssetRaw);
            std::string       shaderLibPath(shaderLibRaw);
            const std::string shaderName = NOUS_FileManager::GetFilename(shaderAssetPath);

            Resource* r = mResourceManager->CreateResourceFromLibrary(
                static_cast<uint32>(shaderUID), ResourceType::SHADER, shaderName,
                shaderAssetPath, shaderLibPath);

            if (ResourceShader* loadedShader = r ? down_cast<ResourceShader*>(r) : nullptr)
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
    }

    json_value_free(rootVal);
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
    for (auto& [name, map] : material->textureMaps)
    {
        if (map.texture)
        {
            mResourceManager->UnloadResource(map.texture->GetUID());
            map.texture = nullptr;
        }
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

    auto updateFile = [&](const std::string& filePath) -> bool
    {
        JSON_Value* val = json_parse_file(filePath.c_str());
        if (!val)
        {
            NOUS_ERROR("ImporterMaterial::SaveMaterialToAssets() — could not open '%s'", filePath.c_str());
            return false;
        }
        JSON_Object* obj = json_value_get_object(val);

        // Write uniforms array (one entry per uniform in the material).
        {
            JSON_Value* uniArrVal = json_value_init_array();
            JSON_Array* uniArr    = json_value_get_array(uniArrVal);
            for (const auto& [name, uv] : material->uniformValues)
            {
                JSON_Value*  entryVal = json_value_init_object();
                JSON_Object* entry    = json_value_get_object(entryVal);
                json_object_set_string(entry, "name", name.c_str());
                json_object_set_string(entry, "type", UniformValueTypeToString(uv.type));

                JSON_Value* valArrVal = json_value_init_array();
                JSON_Array* valArr    = json_value_get_array(valArrVal);
                const uint32_t compCount = UniformValueComponentCountLocal(uv.type);
                for (uint32_t c = 0; c < compCount; ++c)
                    json_array_append_number(valArr, static_cast<double>(uv.data[static_cast<int>(c)]));
                json_object_set_value(entry, "value", valArrVal);

                json_array_append_value(uniArr, entryVal);
            }
            json_object_set_value(obj, "uniforms", uniArrVal);
        }

        // Write texture_maps array (one entry per assigned slot)
        JSON_Value* arrVal = json_value_init_array();
        JSON_Array* arr    = json_value_get_array(arrVal);
        for (const auto& [name, map] : material->textureMaps)
        {
            if (!map.texture) continue;
            // Skip in-memory fallbacks (default checkerboard, white, black, flat-normal)
            // — they have no asset on disk, so persisting an entry for them would
            // produce a broken reference on the next load.
            if (map.texture->GetAssetsPath().empty()) continue;
            JSON_Value*  entryVal = json_value_init_object();
            JSON_Object* entry    = json_value_get_object(entryVal);
            json_object_set_string(entry, "name",         name.c_str());
            json_object_set_string(entry, "asset_path",   map.texture->GetAssetsPath().c_str());
            json_object_set_number(entry, "uid",          static_cast<double>(map.texture->GetUID()));
            json_object_set_string(entry, "library_path", map.texture->GetLibraryPath().c_str());
            json_array_append_value(arr, entryVal);
        }
        json_object_set_value(obj, "texture_maps", arrVal);

        // Shader keys (set or remove)
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

bool ImporterMaterial::CreateNewMaterialFile(const std::string& assetPath)
{
    if (assetPath.empty()) return false;

    // Ensure parent directory exists (e.g. Assets/Materials/).
    NOUS_FileManager::CreateDirectory(NOUS_FileManager::GetDirectory(assetPath));

    JSON_Value*  rootVal = json_value_init_object();
    JSON_Object* root    = json_value_get_object(rootVal);

    // Default uniforms (matching the ForwardBlinnPhong InstanceUBO layout:
    // diffuseColor, emissiveColor, then the four per-channel scalar params).
    JSON_Value* uniArrVal = json_value_init_array();
    JSON_Array* uniArr    = json_value_get_array(uniArrVal);

    auto addVec4Uniform = [&](const char* name, double x, double y, double z, double w)
    {
        JSON_Value*  entryVal = json_value_init_object();
        JSON_Object* entry    = json_value_get_object(entryVal);
        json_object_set_string(entry, "name", name);
        json_object_set_string(entry, "type", "vec4");
        JSON_Value* valArrVal = json_value_init_array();
        JSON_Array* valArr    = json_value_get_array(valArrVal);
        json_array_append_number(valArr, x);
        json_array_append_number(valArr, y);
        json_array_append_number(valArr, z);
        json_array_append_number(valArr, w);
        json_object_set_value(entry, "value", valArrVal);
        json_array_append_value(uniArr, entryVal);
    };

    auto addFloatUniform = [&](const char* name, double value)
    {
        JSON_Value*  entryVal = json_value_init_object();
        JSON_Object* entry    = json_value_get_object(entryVal);
        json_object_set_string(entry, "name", name);
        json_object_set_string(entry, "type", "float");
        JSON_Value* valArrVal = json_value_init_array();
        JSON_Array* valArr    = json_value_get_array(valArrVal);
        json_array_append_number(valArr, value);
        json_object_set_value(entry, "value", valArrVal);
        json_array_append_value(uniArr, entryVal);
    };

    addVec4Uniform("diffuseColor",      1.0, 1.0, 1.0, 1.0);
    addVec4Uniform("emissiveColor",     1.0, 1.0, 1.0, 1.0);
    addFloatUniform("aoIntensity",       1.0);
    addFloatUniform("normalStrength",    1.0);
    addFloatUniform("specularIntensity", 1.0);
    addFloatUniform("shininessScale",    1.0);

    json_object_set_value(root, "uniforms", uniArrVal);

    // Empty texture_maps array — the user will populate it via the Inspector.
    JSON_Value* mapsVal = json_value_init_array();
    json_object_set_value(root, "texture_maps", mapsVal);

    const bool ok = json_serialize_to_file_pretty(rootVal, assetPath.c_str()) == JSONSuccess;
    json_value_free(rootVal);
    return ok;
}
