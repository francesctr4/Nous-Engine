#include "Engine/Systems/ResourceManager/ResourceTypes/ResourceMaterial/include/ImporterMaterial.h"

#include "Engine/Systems/ResourceManager/ResourceTypes/ResourceMaterial/include/ResourceMaterial.h"
#include "Engine/Systems/ResourceManager/ResourceTypes/ResourceTexture/include/ResourceTexture.h"
#include "Engine/Systems/ResourceManager/ResourceTypes/ResourceShader/include/ResourceShader.h"
#include "Engine/Modules/ModuleResourceManager/include/ModuleResourceManager.h"
#include "Engine/Renderer/IGPUResourceFactory.h"

#include "Engine/Core/FileSystem/FileSystem.h"
#include "Engine/Utils/Serialization/JsonFile/JsonFile.h"
#include "Engine/Utils/Serialization/JsonFile/JsonObject.h"
#include "Engine/Utils/Serialization/JsonFile/JsonArray.h"
#include "Engine/Systems/ResourceManager/Core/Resource/MetaFileData.inl"
#include <algorithm>

#include "Engine/Core/MemoryManager/MemoryManager.h"

#include "Engine/Core/Logger/Logger.h"

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

static UniformValueType StringToUniformValueType(std::string_view str)
{
    if (str == "float") return UniformValueType::Float;
    if (str == "vec2")  return UniformValueType::Vec2;
    if (str == "vec3")  return UniformValueType::Vec3;
    if (str == "vec4")  return UniformValueType::Vec4;
    if (str == "int")   return UniformValueType::Int;
    if (str == "ivec2") return UniformValueType::IVec2;
    if (str == "ivec3") return UniformValueType::IVec3;
    if (str == "ivec4") return UniformValueType::IVec4;
    return UniformValueType::Vec4;
}


bool ImporterMaterial::Import(const MetaFileData& metaFileData)
{
    Resource* tempMaterial = NOUS_NEW<ResourceMaterial>(MemoryTag::RESOURCE_MATERIAL);
    return Save(metaFileData, tempMaterial);
}

bool ImporterMaterial::Save(const MetaFileData& metaFileData, Resource*& inResource)
{
    NOUS_DELETE(inResource, MemoryTag::RESOURCE_MATERIAL);

    JsonObject root = JsonFile::LoadFromFile(metaFileData.assetsPath);
    if (root.IsEmpty())
        return nous::engine::filesystem::CopyFile(metaFileData.assetsPath, metaFileData.libraryPath);

    // ── Enrich each entry in the texture_maps array with uid + library_path ──
    {
        JsonArray origMaps = root.GetArray("texture_maps");
        if (!origMaps.IsEmpty())
        {
            JsonArray newMaps;
            const int count = origMaps.Count();
            for (int i = 0; i < count; ++i)
            {
                JsonObject entry = origMaps.GetObject(i);
                if (entry.IsEmpty()) continue;

                const std::string rawPath = entry.GetString("asset_path");
                if (!rawPath.empty())
                {
                    const std::string texPath = nous::engine::filesystem::NormalizePath(rawPath);
                    MetaFileData texMeta;
                    if (ImportPipeline::GetAssetMetaData(texPath, texMeta))
                    {
                        entry.Set("uid",          static_cast<double>(texMeta.uid));
                        entry.Set("library_path", nous::engine::filesystem::NormalizePath(texMeta.libraryPath));
                    }
                }
                newMaps.Append(std::move(entry));
            }
            root.Set("texture_maps", std::move(newMaps));
        }
    }

    // ── Enrich shader_asset_path with shader_uid + shader_library_path ──
    {
        const std::string rawShaderPath = root.GetString("shader_asset_path");
        if (!rawShaderPath.empty())
        {
            const std::string shaderPath = nous::engine::filesystem::NormalizePath(rawShaderPath);
            MetaFileData shaderMeta;
            if (ImportPipeline::GetAssetMetaData(shaderPath, shaderMeta))
            {
                root.Set("shader_uid",          static_cast<double>(shaderMeta.uid));
                root.Set("shader_library_path", nous::engine::filesystem::NormalizePath(shaderMeta.libraryPath));
            }
        }
    }

    return JsonFile::SaveToFile(root, metaFileData.libraryPath);
}

// ---------------------------------------------------------------------------
// Deserialize helpers
// ---------------------------------------------------------------------------

static void DeserializeUniforms(const JsonObject& root, ResourceMaterial* material)
{
    // Data-driven: every uniform must live in the "uniforms" array. Members the
    // material doesn't declare are filled with reflection defaults at draw time.
    JsonArray uniformsArr = root.GetArray("uniforms");
    if (uniformsArr.IsEmpty()) return;

    const int count = uniformsArr.Count();
    for (int i = 0; i < count; ++i)
    {
        JsonObject entry = uniformsArr.GetObject(i);
        if (entry.IsEmpty()) continue;

        const std::string name    = entry.GetString("name");
        const std::string typeStr = entry.GetString("type");
        JsonArray         valArr  = entry.GetArray("value");
        if (name.empty() || valArr.IsEmpty()) continue;

        UniformValue uv = UniformValue::MakeDefault(StringToUniformValueType(typeStr));

        const uint32_t compCount = UniformValueComponentCount(uv.type);
        const int      arrCount  = valArr.Count();
        if (UniformValueIsInt(uv.type))
        {
            for (uint32_t c = 0; c < compCount && static_cast<int>(c) < arrCount; ++c)
                uv.idata[static_cast<int>(c)] = static_cast<int32_t>(valArr.GetDouble(static_cast<int>(c)));
        }
        else
        {
            for (uint32_t c = 0; c < compCount && static_cast<int>(c) < arrCount; ++c)
                uv.fdata[static_cast<int>(c)] = valArr.GetFloat(static_cast<int>(c));
        }

        material->uniformValues[name] = uv;
    }
}

static void DeserializeTextureMaps(const JsonObject& root, ResourceMaterial* material,
                                    ModuleResourceManager* rm)
{
    // Data-driven: every texture slot lives in the "texture_maps" array with
    // name + asset_path + uid + library_path. Save() enriches uid/library_path
    // at import time, so library copies always carry them.
    JsonArray texMapsArr = root.GetArray("texture_maps");
    if (texMapsArr.IsEmpty()) return;

    const int count = texMapsArr.Count();
    for (int i = 0; i < count; ++i)
    {
        JsonObject entry = texMapsArr.GetObject(i);
        if (entry.IsEmpty()) continue;

        const std::string name      = entry.GetString("name");
        const std::string rawPath   = entry.GetString("asset_path");
        const std::string libRaw    = entry.GetString("library_path");
        const double      uidDouble = entry.GetDouble("uid", 0.0);
        if (name.empty() || rawPath.empty() || libRaw.empty() || uidDouble == 0.0)
        {
            NOUS_WARN("ImporterMaterial::Deserialize() — texture entry missing name/asset_path/uid/library_path; skipping.");
            continue;
        }

        const std::string assetPath = nous::engine::filesystem::NormalizePath(rawPath);
        const std::string libPath   = nous::engine::filesystem::NormalizePath(libRaw);

        const uint32      texUID  = static_cast<uint32>(uidDouble);
        const std::string texName = nous::engine::filesystem::GetFilename(assetPath);
        ResourceTexture*  tex     = down_cast<ResourceTexture*>(
            rm->CreateResourceFromLibrary(texUID, ResourceType::TEXTURE, texName, assetPath, libPath));

        if (tex)
            material->textureMaps[name].texture = tex;
        else
            NOUS_WARN("ImporterMaterial::Deserialize() — texture '%s' (slot '%s') could not be loaded.",
                      assetPath.c_str(), name.c_str());
    }
}

static void DeserializeShader(const JsonObject& root, ResourceMaterial* material,
                               ModuleResourceManager* rm)
{
    // Data-driven: shader_asset_path + shader_uid + shader_library_path are all
    // required. Save() enriches uid/library_path at import time.
    const std::string shaderAssetRaw = root.GetString("shader_asset_path");
    if (shaderAssetRaw.empty()) return;

    const std::string shaderLibRaw = root.GetString("shader_library_path");
    const double      shaderUID    = root.GetDouble("shader_uid", 0.0);
    if (shaderLibRaw.empty() || shaderUID == 0.0)
    {
        NOUS_WARN("ImporterMaterial::Deserialize() — shader entry missing shader_uid/shader_library_path; falling back to built-in.");
        return;
    }

    const std::string shaderName = nous::engine::filesystem::GetFilename(shaderAssetRaw);
    Resource* r = rm->CreateResourceFromLibrary(
        static_cast<uint32>(shaderUID), ResourceType::SHADER, shaderName,
        shaderAssetRaw, shaderLibRaw);

    if (ResourceShader* loadedShader = r ? down_cast<ResourceShader*>(r) : nullptr)
    {
        material->SetShader(loadedShader);
    }
    else
    {
        NOUS_WARN("ImporterMaterial::Deserialize() — shader '%s' could not be loaded; falling back to built-in.",
                  shaderAssetRaw.c_str());
    }
}

// ---------------------------------------------------------------------------

bool ImporterMaterial::Deserialize(const std::string& libraryPath, Resource* outResource)
{
    ResourceMaterial* material = down_cast<ResourceMaterial*>(outResource);

    JsonObject root = JsonFile::LoadFromFile(libraryPath);
    if (root.IsEmpty())
    {
        NOUS_ERROR("ImporterMaterial::Deserialize() failed to load file '%s'", libraryPath.c_str());
        return false;
    }

    DeserializeUniforms(root, material);
    DeserializeTextureMaps(root, material, mResourceManager);
    DeserializeShader(root, material, mResourceManager);

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
        material->SetShader(nullptr);
    }
}

bool ImporterMaterial::SaveMaterialToAssets(ResourceMaterial* material)
{
    if (!material) return false;

    auto buildUniformsArray = [&]() -> JsonArray
    {
        std::vector<std::string> keys;
        keys.reserve(material->uniformValues.size());
        for (const auto& [name, uv] : material->uniformValues)
            keys.push_back(name);
        std::sort(keys.begin(), keys.end());

        JsonArray uniArr;
        for (const auto& name : keys)
        {
            const auto& uv = material->uniformValues.at(name);
            JsonObject entry;
            entry.Set("name", name);
            entry.Set("type", UniformValueTypeToString(uv.type));

            JsonArray valArr;
            const uint32_t compCount = UniformValueComponentCount(uv.type);
            if (UniformValueIsInt(uv.type))
            {
                for (uint32_t c = 0; c < compCount; ++c)
                    valArr.Append(static_cast<double>(uv.idata[static_cast<int>(c)]));
            }
            else
            {
                for (uint32_t c = 0; c < compCount; ++c)
                    valArr.Append(static_cast<double>(uv.fdata[static_cast<int>(c)]));
            }
            entry.Set("value", std::move(valArr));
            uniArr.Append(std::move(entry));
        }
        return uniArr;
    };

    auto buildTextureMapsArray = [&]() -> JsonArray
    {
        std::vector<std::string> keys;
        keys.reserve(material->textureMaps.size());
        for (const auto& [name, map] : material->textureMaps)
            keys.push_back(name);
        std::sort(keys.begin(), keys.end());

        JsonArray arr;
        for (const auto& name : keys)
        {
            const auto& map = material->textureMaps.at(name);
            if (!map.texture) continue;
            // Skip in-memory fallbacks (default checkerboard, white, black, flat-normal)
            // — they have no asset on disk, so persisting an entry for them would
            // produce a broken reference on the next load.
            if (map.texture->GetAssetsPath().empty()) continue;
            JsonObject entry;
            entry.Set("name",         name);
            entry.Set("asset_path",   map.texture->GetAssetsPath());
            entry.Set("uid",          static_cast<double>(map.texture->GetUID()));
            entry.Set("library_path", map.texture->GetLibraryPath());
            arr.Append(std::move(entry));
        }
        return arr;
    };

    auto updateFile = [&](const std::string& filePath) -> bool
    {
        JsonObject obj = JsonFile::LoadFromFile(filePath);
        if (obj.IsEmpty())
        {
            NOUS_ERROR("ImporterMaterial::SaveMaterialToAssets() — could not open '%s'", filePath.c_str());
            return false;
        }

        obj.Set("uniforms",     buildUniformsArray());
        obj.Set("texture_maps", buildTextureMapsArray());

        if (material->shader)
        {
            obj.Set("shader_uid",          static_cast<double>(material->shaderUID));
            obj.Set("shader_asset_path",   material->shader->GetAssetsPath());
            obj.Set("shader_library_path", material->shader->GetLibraryPath());
        }
        else
        {
            obj.Remove("shader_uid");
            obj.Remove("shader_asset_path");
            obj.Remove("shader_library_path");
        }

        return JsonFile::SaveToFile(obj, filePath);
    };

    bool ok = updateFile(material->GetAssetsPath());
    ok     &= updateFile(material->GetLibraryPath());
    return ok;
}

bool ImporterMaterial::CreateNewMaterialFile(const std::string& assetPath)
{
    if (assetPath.empty()) return false;

    // Ensure parent directory exists (e.g. Assets/Materials/).
    nous::engine::filesystem::CreateDirectory(nous::engine::filesystem::GetDirectory(assetPath));

    auto makeVec4Uniform = [](const char* name, double x, double y, double z, double w) -> JsonObject
    {
        JsonObject entry;
        entry.Set("name", name);
        entry.Set("type", "vec4");
        JsonArray valArr;
        valArr.Append(x); valArr.Append(y); valArr.Append(z); valArr.Append(w);
        entry.Set("value", std::move(valArr));
        return entry;
    };

    auto makeFloatUniform = [](const char* name, double value) -> JsonObject
    {
        JsonObject entry;
        entry.Set("name", name);
        entry.Set("type", "float");
        JsonArray valArr;
        valArr.Append(value);
        entry.Set("value", std::move(valArr));
        return entry;
    };

    // Default uniforms (matching the ForwardBlinnPhong InstanceUBO layout).
    JsonArray uniArr;
    uniArr.Append(makeVec4Uniform("diffuseColor",      1.0, 1.0, 1.0, 1.0));
    uniArr.Append(makeVec4Uniform("emissiveColor",     1.0, 1.0, 1.0, 1.0));
    uniArr.Append(makeFloatUniform("aoIntensity",       1.0));
    uniArr.Append(makeFloatUniform("normalStrength",    1.0));
    uniArr.Append(makeFloatUniform("specularIntensity", 1.0));
    uniArr.Append(makeFloatUniform("shininessScale",    1.0));

    JsonObject root;
    root.Set("uniforms",     std::move(uniArr));
    root.Set("texture_maps", JsonArray{});  // empty — user will populate via Inspector

    return JsonFile::SaveToFile(root, assetPath);
}
