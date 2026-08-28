#include <ResourceManager/Types/ResourceMesh/ImporterMesh.h>
#include <EngineCore/InvalidID.h>
#include <EngineCore/Casts.h>
#include <FileSystem/FileHandle.h>

#include <map>

#include <ResourceManager/Types/ResourceMesh/ResourceMesh.h>
#include <ResourceManager/Core/MetaFileData.h>

#include <MemoryManager/MemoryManager.h>

#include <Renderer/IGPUResourceFactory.h>
#include <ResourceManager/Core/IResourceLoader.h>
#include <ResourceManager/Types/ResourceMaterial/ResourceMaterial.h>
#include <ResourceManager/Types/ResourceTexture/ResourceTexture.h>

#include <Logger/Logger.h>

#include <Utils/Serialization/JsonFile.h>
#include <Utils/Serialization/JsonObject.h>
#include <Utils/Serialization/JsonArray.h>

// Assimp
#define ASSIMP_LOAD_FLAGS (aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_JoinIdenticalVertices | aiProcess_CalcTangentSpace)
#include "assimp/scene.h"
#include "assimp/cimport.h"
#include "assimp/postprocess.h"
#include "assimp/material.h"

#include <filesystem>
#include <unordered_set>
#include <unordered_map>

// ─── Binary format ────────────────────────────────────────────────────────────
// V1 (legacy): uint64_t vertexCount | Vertex3D[] | uint64_t indexCount | uint32_t[]
// V2 (legacy): uint32_t magic | uint32_t submeshCount | N×(nameLen:u64, name:chars,
//              localTransform:16×float, vertexCount:u64, Vertex3D[],
//              indexCount:u64, uint32_t[])
// V3 (current): same as V2 plus a per-submesh material reference inserted between
//               localTransform and vertexCount:
//               ... localTransform:16×float, matPathLen:u64, matPath:chars,
//               vertexCount:u64, Vertex3D[], ...
static constexpr uint32_t MESH_BINARY_MAGIC_V2 = 0xFA7C0DE1u;
static constexpr uint32_t MESH_BINARY_MAGIC_V3 = 0xFA7C0DE2u;

// ─── Assimp helpers ───────────────────────────────────────────────────────────

static glm::mat4 AiToGlm(const aiMatrix4x4& m)
{
    // Assimp is row-major; GLM is column-major.
    return glm::mat4(
        m.a1, m.b1, m.c1, m.d1,   // column 0
        m.a2, m.b2, m.c2, m.d2,   // column 1
        m.a3, m.b3, m.c3, m.d3,   // column 2
        m.a4, m.b4, m.c4, m.d4    // column 3
    );
}

// Weld smooth normals per position for the outline pass.
// For each vertex, accumulates and averages the face normals of all vertices that
// share the same position, then normalizes the result into smoothNormal.
static void WeldSmoothNormals(SubMeshData& out, size_t startIdx)
{
    struct Vec3Less {
        bool operator()(const glm::vec3& a, const glm::vec3& b) const {
            if (a.x != b.x) return a.x < b.x;
            if (a.y != b.y) return a.y < b.y;
            return a.z < b.z;
        }
    };
    std::map<glm::vec3, std::pair<glm::vec3, uint32_t>, Vec3Less> accum;
    for (size_t i = startIdx; i < out.vertices.size(); ++i) {
        auto& [sum, cnt] = accum[out.vertices[i].position];
        sum += out.vertices[i].normal;
        ++cnt;
    }
    for (size_t i = startIdx; i < out.vertices.size(); ++i) {
        const auto& [sum, cnt] = accum[out.vertices[i].position];
        out.vertices[i].smoothNormal = glm::normalize(sum / static_cast<float>(cnt));
    }
}

// Fill one SubMeshData from an aiMesh.  smoothNormals are welded per position.
// materialPaths is indexed by aiMaterial index; the .nmat path for this aiMesh
// is looked up via mesh->mMaterialIndex and stamped into out.materialAssetPath.
// Pass an empty vector to skip material wiring (legacy / save path).
static void ExtractSubMesh(aiMesh* mesh, const glm::mat4& transform,
                            const std::string& name,
                            const std::vector<std::string>& materialPaths,
                            SubMeshData& out)
{
    out.name           = name;
    out.localTransform = transform;

    if (mesh->mMaterialIndex < materialPaths.size())
        out.materialAssetPath = materialPaths[mesh->mMaterialIndex];

    const size_t startIdx = out.vertices.size();

    for (uint32_t i = 0; i < mesh->mNumVertices; ++i)
    {
        Vertex3D vertex;

        vertex.position = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
        vertex.normal   = { mesh->mNormals[i].x,  mesh->mNormals[i].y,  mesh->mNormals[i].z  };

        vertex.color = mesh->HasVertexColors(0)
            ? glm::vec3(mesh->mColors[0][i].r, mesh->mColors[0][i].g, mesh->mColors[0][i].b)
            : glm::vec3(1.0f);

        vertex.texCoord = mesh->HasTextureCoords(0)
            ? glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y)
            : glm::vec2(0.0f);

        vertex.smoothNormal = { 0.0f, 0.0f, 0.0f }; // computed below

        if (mesh->HasTangentsAndBitangents())
        {
            const glm::vec3 t = { mesh->mTangents[i].x,   mesh->mTangents[i].y,   mesh->mTangents[i].z   };
            const glm::vec3 b = { mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z };
            const float sign  = (glm::dot(glm::cross(vertex.normal, t), b) < 0.0f) ? -1.0f : 1.0f;
            vertex.tangent    = glm::vec4(t, sign);
        }
        else
        {
            vertex.tangent = { 1.0f, 0.0f, 0.0f, 1.0f };
        }

        vertex.texCoord2 = mesh->HasTextureCoords(1)
            ? glm::vec2(mesh->mTextureCoords[1][i].x, mesh->mTextureCoords[1][i].y)
            : glm::vec2(0.0f);

        out.vertices.emplace_back(vertex);
    }

    WeldSmoothNormals(out, startIdx);

    if (mesh->HasFaces())
    {
        for (uint32_t i = 0; i < mesh->mNumFaces; ++i)
        {
            const aiFace& face = mesh->mFaces[i];
            for (uint32_t j = 0; j < face.mNumIndices; ++j)
                out.indices.push_back(static_cast<uint32_t>(face.mIndices[j]));
        }
    }
}

// Recursively traverse nodes, accumulating the world transform.
// Each (node, meshIndex) pair becomes one SubMeshData.
// materialPaths is indexed by aiMaterial index and forwarded to ExtractSubMesh.
static void CollectSubMeshData(aiNode* node, const aiScene* scene,
                                const glm::mat4& parentTransform,
                                const std::vector<std::string>& materialPaths,
                                std::vector<SubMeshData>& outSubmeshes)
{
    const glm::mat4 nodeTransform = parentTransform * AiToGlm(node->mTransformation);

    for (uint32_t i = 0; i < node->mNumMeshes; ++i)
    {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];

        std::string name = mesh->mName.length > 0
            ? std::string(mesh->mName.C_Str())
            : (std::string(node->mName.C_Str()) + "_" + std::to_string(i));

        SubMeshData sub;
        ExtractSubMesh(mesh, nodeTransform, name, materialPaths, sub);
        outSubmeshes.emplace_back(std::move(sub));
    }

    for (uint32_t i = 0; i < node->mNumChildren; ++i)
        CollectSubMeshData(node->mChildren[i], scene, nodeTransform, materialPaths, outSubmeshes);
}

// ─── Write helpers ────────────────────────────────────────────────────────────

static bool WriteU32(FileHandle& fh, uint32_t v)
{
    uint64_t w = 0; return fh.Write(4, &v, &w);
}
static bool WriteU64(FileHandle& fh, uint64_t v)
{
    uint64_t w = 0; return fh.Write(8, &v, &w);
}
static bool WriteBytes(FileHandle& fh, const void* data, uint64_t size)
{
    uint64_t w = 0; return fh.Write(size, data, &w);
}

// Write a V3 multi-submesh binary from a pre-collected vector of SubMeshData.
static bool SaveSubmeshes(const MetaFileData& metaFileData,
                          const std::vector<SubMeshData>& submeshes)
{
    FileHandle fh;
    if (!fh.Open(metaFileData.libraryPath, FileMode::WRITE, true))
        return false;

    bool ok = true;
    ok &= WriteU32(fh, MESH_BINARY_MAGIC_V3);
    ok &= WriteU32(fh, static_cast<uint32_t>(submeshes.size()));

    for (const auto& sub : submeshes)
    {
        // Name
        const uint64_t nameLen = sub.name.size();
        ok &= WriteU64(fh, nameLen);
        if (nameLen > 0)
            ok &= WriteBytes(fh, sub.name.data(), nameLen);

        // Local transform (16 floats, column-major)
        ok &= WriteBytes(fh, &sub.localTransform[0][0], 16 * sizeof(float));

        // Material asset path (V3+ only; empty string is valid → falls back to
        // default material at spawn time).
        const uint64_t matPathLen = sub.materialAssetPath.size();
        ok &= WriteU64(fh, matPathLen);
        if (matPathLen > 0)
            ok &= WriteBytes(fh, sub.materialAssetPath.data(), matPathLen);

        // Vertices
        const uint64_t vCount = sub.vertices.size();
        ok &= WriteU64(fh, vCount);
        if (vCount > 0)
            ok &= WriteBytes(fh, sub.vertices.data(), vCount * sizeof(Vertex3D));

        // Indices
        const uint64_t iCount = sub.indices.size();
        ok &= WriteU64(fh, iCount);
        if (iCount > 0)
            ok &= WriteBytes(fh, sub.indices.data(), iCount * sizeof(uint32_t));
    }

    fh.Close();
    return ok;
}

// ─── Binary parser ────────────────────────────────────────────────────────────
// Shared by Deserialize (which merges submeshes) and LoadHierarchy (which keeps them separate).
// Returns one SubMeshData per logical submesh; empty on open/read failure.

static std::vector<SubMeshData> ParseMeshBinary(const std::string& libraryPath)
{
    std::vector<SubMeshData> result;

    FileHandle fh;
    if (!fh.Open(libraryPath, FileMode::READ, true))
    {
        NOUS_ERROR("ParseMeshBinary: failed to open '%s'", libraryPath.c_str());
        return result;
    }

    uint64_t bytesRead = 0;

    uint32_t header4 = 0;
    if (!fh.ReadBytes(4, reinterpret_cast<char*>(&header4), &bytesRead))
        return result;

    const bool isV3 = (header4 == MESH_BINARY_MAGIC_V3);
    const bool isV2 = (header4 == MESH_BINARY_MAGIC_V2);

    if (!isV2 && !isV3)
    {
        // V1 format: single flat mesh — wrap in one SubMeshData
        uint32_t header4High = 0;
        fh.ReadBytes(4, reinterpret_cast<char*>(&header4High), &bytesRead);
        const uint64_t vCount = header4 | (static_cast<uint64_t>(header4High) << 32);

        SubMeshData sub;
        sub.name           = "Mesh";
        sub.localTransform = glm::mat4(1.0f);
        sub.vertices.resize(vCount);
        fh.ReadBytes(vCount * sizeof(Vertex3D), reinterpret_cast<char*>(sub.vertices.data()), &bytesRead);

        uint64_t iCount = 0;
        fh.ReadBytes(sizeof(iCount), reinterpret_cast<char*>(&iCount), &bytesRead);
        sub.indices.resize(iCount);
        fh.ReadBytes(iCount * sizeof(uint32_t), reinterpret_cast<char*>(sub.indices.data()), &bytesRead);

        fh.Close();
        result.emplace_back(std::move(sub));
        return result;
    }

    // V2 / V3 format
    uint32_t submeshCount = 0;
    if (!fh.ReadBytes(4, reinterpret_cast<char*>(&submeshCount), &bytesRead))
        return result;

    result.reserve(submeshCount);

    for (uint32_t s = 0; s < submeshCount; ++s)
    {
        SubMeshData sub;

        uint64_t nameLen = 0;
        fh.ReadBytes(sizeof(nameLen), reinterpret_cast<char*>(&nameLen), &bytesRead);
        if (nameLen > 0)
        {
            sub.name.resize(nameLen);
            fh.ReadBytes(nameLen, sub.name.data(), &bytesRead);
        }

        fh.ReadBytes(16 * sizeof(float), reinterpret_cast<char*>(&sub.localTransform[0][0]), &bytesRead);

        if (isV3)
        {
            uint64_t matPathLen = 0;
            fh.ReadBytes(sizeof(matPathLen), reinterpret_cast<char*>(&matPathLen), &bytesRead);
            if (matPathLen > 0)
            {
                sub.materialAssetPath.resize(matPathLen);
                fh.ReadBytes(matPathLen, sub.materialAssetPath.data(), &bytesRead);
            }
        }

        uint64_t vCount = 0;
        fh.ReadBytes(sizeof(vCount), reinterpret_cast<char*>(&vCount), &bytesRead);
        sub.vertices.resize(vCount);
        fh.ReadBytes(vCount * sizeof(Vertex3D), reinterpret_cast<char*>(sub.vertices.data()), &bytesRead);

        uint64_t iCount = 0;
        fh.ReadBytes(sizeof(iCount), reinterpret_cast<char*>(&iCount), &bytesRead);
        sub.indices.resize(iCount);
        fh.ReadBytes(iCount * sizeof(uint32_t), reinterpret_cast<char*>(sub.indices.data()), &bytesRead);

        result.emplace_back(std::move(sub));
    }

    fh.Close();
    return result;
}

// ─── Importer interface ───────────────────────────────────────────────────────

// Map an assimp texture type to a sampler name in ForwardBlinnPhong.glsl.
// Returns nullptr for slots the shader does not consume — those textures are still
// imported (so they appear in AssetsBrowser) but are not wired into the .nmat.
static const char* AssimpTypeToSamplerName(aiTextureType type)
{
    switch (type)
    {
        case aiTextureType_DIFFUSE:
        case aiTextureType_BASE_COLOR:        return "diffuseSampler";
        case aiTextureType_NORMALS:
        case aiTextureType_NORMAL_CAMERA:     return "normalSampler";
        case aiTextureType_SPECULAR:          return "specularSampler";
        case aiTextureType_SHININESS:         return "shininessSampler";
        case aiTextureType_AMBIENT_OCCLUSION:
        case aiTextureType_LIGHTMAP:          return "aoSampler";
        case aiTextureType_EMISSIVE:
        case aiTextureType_EMISSION_COLOR:    return "emissiveSampler";
        default:                              return nullptr;
    }
}

// Replace characters that would be awkward in a filename. Keeps alnum, dash, dot,
// underscore. Everything else becomes '_'.
static std::string SanitizeFilenamePart(const std::string& in)
{
    std::string out;
    out.reserve(in.size());
    for (char c : in)
    {
        const bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
                        (c >= 'A' && c <= 'Z') || c == '-' || c == '_' || c == '.';
        out.push_back(ok ? c : '_');
    }
    if (out.empty()) out = "Material";
    return out;
}

// Walk every material's texture slots, import each referenced image, and emit a
// sibling .nmat per material wiring those textures into ForwardBlinnPhong sampler
// slots. Two texture-reference flavors handled:
//   - External path (typical for .gltf/.fbx/.obj): resolve relative to the model's
//     directory; if the sibling file exists, hand it to ImportFile (idempotent).
//   - Embedded "*N" reference (typical for .glb): pull the bytes out of
//     scene->mTextures[N] and write them next to the model with a generated name,
//     then ImportFile that. Uncompressed (raw RGBA) embeds are rare and would need
//     stb_image_write to round-trip to PNG — skipped here with a warning.
// Existing .nmat files are never overwritten — re-importing the model preserves
// any tweaks the user made in the Inspector.
// Returns a vector indexed by aiMaterial index, where each entry is the Assets/-
// relative path of the generated .nmat (or empty if no material file was emitted —
// e.g. all texture slots were empty or referenced missing files). The caller uses
// this to stamp per-submesh materialAssetPath via aiMesh::mMaterialIndex.
static std::vector<std::string> ExtractTexturesAndMaterials(const aiScene* scene,
                                        const std::string& modelAssetPath,
                                        IResourceLoader* rm)
{
    std::vector<std::string> materialPaths;
    if (!rm || !scene || scene->mNumMaterials == 0) return materialPaths;

    materialPaths.assign(scene->mNumMaterials, std::string{});

    namespace fs = std::filesystem;
    const fs::path    modelPath(modelAssetPath);
    const fs::path    modelDir  = modelPath.parent_path();
    const std::string modelStem = modelPath.stem().string();

    constexpr const char* k_DefaultShaderPath = "Assets/Shaders/ForwardBlinnPhong.glsl";

    std::unordered_set<std::string> seenTextures;
    std::unordered_set<std::string> usedMatFilenames;

    for (uint32_t m = 0; m < scene->mNumMaterials; ++m)
    {
        aiMaterial* mat = scene->mMaterials[m];
        if (!mat) continue;

        // First sampler→asset_path wins (multiple assimp types can map to the
        // same shader sampler; e.g. BASE_COLOR + DIFFUSE both → diffuseSampler).
        std::unordered_map<std::string, std::string> samplerToAssetPath;

        for (int t = aiTextureType_NONE + 1; t < aiTextureType_UNKNOWN; ++t)
        {
            const auto     type  = static_cast<aiTextureType>(t);
            const uint32_t count = mat->GetTextureCount(type);

            for (uint32_t i = 0; i < count; ++i)
            {
                aiString aiTexPath;
                if (mat->GetTexture(type, i, &aiTexPath) != aiReturn_SUCCESS)
                    continue;

                const std::string ref = aiTexPath.C_Str();
                if (ref.empty()) continue;

                std::string finalAssetPath;

                if (ref[0] == '*')
                {
                    const int idx = std::atoi(ref.c_str() + 1);
                    if (idx < 0 || static_cast<uint32_t>(idx) >= scene->mNumTextures) continue;
                    const aiTexture* aiTex = scene->mTextures[idx];
                    if (!aiTex) continue;

                    if (aiTex->mHeight != 0)
                    {
                        NOUS_WARN("ImporterMesh: skipping uncompressed embedded texture in '%s' (slot %d/%d) — RGBA-to-PNG conversion not implemented.",
                            modelAssetPath.c_str(), t, i);
                        continue;
                    }

                    const std::string ext  = aiTex->achFormatHint[0] ? (std::string(".") + aiTex->achFormatHint) : std::string(".png");
                    const std::string name = modelStem + "_mat" + std::to_string(m) + "_tex" + std::to_string(t) + "_" + std::to_string(i) + ext;
                    const fs::path    out  = modelDir / name;
                    finalAssetPath = out.generic_string();

                    if (!fs::exists(out))
                    {
                        FileHandle fh;
                        if (!fh.Open(finalAssetPath, FileMode::WRITE, true))
                        {
                            NOUS_WARN("ImporterMesh: failed to write embedded texture '%s'.", finalAssetPath.c_str());
                            continue;
                        }
                        uint64_t written = 0;
                        fh.Write(aiTex->mWidth, aiTex->pcData, &written);
                        fh.Close();
                    }
                }
                else
                {
                    fs::path texFsPath(ref);
                    if (texFsPath.is_relative())
                        texFsPath = modelDir / texFsPath;
                    finalAssetPath = texFsPath.generic_string();

                    if (!fs::exists(texFsPath))
                    {
                        NOUS_WARN("ImporterMesh: referenced texture not found, skipping: '%s'.", finalAssetPath.c_str());
                        continue;
                    }
                }

                if (seenTextures.insert(finalAssetPath).second)
                    rm->ImportFile(finalAssetPath);

                if (const char* samplerName = AssimpTypeToSamplerName(type))
                {
                    auto [it, inserted] = samplerToAssetPath.try_emplace(samplerName, finalAssetPath);
                    (void)it;
                    (void)inserted;
                }
            }
        }

        // Resolve a per-material filename. Assimp materials are not guaranteed to
        // have unique names within a scene, so we disambiguate with the index.
        aiString matNameAi;
        mat->Get(AI_MATKEY_NAME, matNameAi);
        std::string matName = (matNameAi.length > 0) ? std::string(matNameAi.C_Str())
                                                     : ("Material_" + std::to_string(m));
        std::string filenameStem = modelStem + "_" + SanitizeFilenamePart(matName);
        if (!usedMatFilenames.insert(filenameStem).second)
            filenameStem += "_" + std::to_string(m);

        const std::string matAssetPath = (modelDir / (filenameStem + ".nmat")).generic_string();

        // Always record the path so submeshes can resolve to this material, even if
        // we don't (re)write the .nmat below — e.g. a pre-existing user-edited file.
        materialPaths[m] = matAssetPath;

        // Skip — never overwrite a user-edited material.
        if (fs::exists(matAssetPath)) continue;

        // Build defaults matching ForwardBlinnPhong's InstanceUBO layout (mirrors
        // ImporterMaterial::CreateNewMaterialFile so the Inspector sees the same
        // defaults the user would get from the "Create Material" menu).
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

        JsonArray uniArr;
        uniArr.Append(makeVec4Uniform ("diffuseColor",      1.0, 1.0, 1.0, 1.0));
        uniArr.Append(makeVec4Uniform ("emissiveColor",     1.0, 1.0, 1.0, 1.0));
        uniArr.Append(makeFloatUniform("aoIntensity",       1.0));
        uniArr.Append(makeFloatUniform("normalStrength",    1.0));
        uniArr.Append(makeFloatUniform("specularIntensity", 1.0));
        uniArr.Append(makeFloatUniform("shininessScale",    1.0));

        JsonArray texMapsArr;
        for (const auto& [samplerName, assetPath] : samplerToAssetPath)
        {
            JsonObject entry;
            entry.Set("name",       samplerName);
            entry.Set("asset_path", assetPath);
            texMapsArr.Append(std::move(entry));
        }

        JsonObject root;
        root.Set("uniforms",          std::move(uniArr));
        root.Set("texture_maps",      std::move(texMapsArr));
        root.Set("shader_asset_path", k_DefaultShaderPath);

        if (!JsonFile::SaveToFile(root, matAssetPath))
        {
            NOUS_WARN("ImporterMesh: failed to write generated material '%s'.", matAssetPath.c_str());
            continue;
        }

        rm->ImportFile(matAssetPath);
        NOUS_INFO("ImporterMesh: generated material '%s' (%zu texture slot(s)).",
                  matAssetPath.c_str(), samplerToAssetPath.size());
    }

    return materialPaths;
}

bool ImporterMesh::Import(const MetaFileData& metaFileData)
{
    const aiScene* scene = aiImportFile(metaFileData.assetsPath.c_str(), ASSIMP_LOAD_FLAGS);

    if (!scene || !scene->HasMeshes())
    {
        NOUS_ERROR("Failed to load 3D Model: %s", metaFileData.assetsPath.c_str());
        return false;
    }

    // Pull textures referenced by the model into Assets/ + Library/ so they appear
    // in AssetsBrowser, and emit one sibling .nmat per assimp material wired to
    // ForwardBlinnPhong. Existing .nmat files are skipped (user edits preserved).
    // The returned vector maps aiMaterial index → .nmat asset path; threaded into
    // CollectSubMeshData so each SubMeshData carries the path of its material.
    //
    // Gated to .gltf / .glb only: glTF has a well-defined PBR material spec so
    // assimp's reported texture slots are reliable. FBX/OBJ/DAE slot mapping is
    // inconsistent across DCC tools and tends to spam wrong slots, so submeshes
    // from those formats fall back to the default material at spawn time.
    std::string ext = std::filesystem::path(metaFileData.assetsPath).extension().string();
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    std::vector<std::string> materialPaths;
    if (ext == ".gltf" || ext == ".glb")
        materialPaths = ExtractTexturesAndMaterials(scene, metaFileData.assetsPath, m_resources);

    std::vector<SubMeshData> submeshes;
    CollectSubMeshData(scene->mRootNode, scene, glm::mat4(1.0f), materialPaths, submeshes);
    aiReleaseImport(scene);

    if (submeshes.empty())
    {
        NOUS_ERROR("No submeshes found in: %s", metaFileData.assetsPath.c_str());
        return false;
    }

    return SaveSubmeshes(metaFileData, submeshes);
}

bool ImporterMesh::Save(const MetaFileData& metaFileData, ResourceBase*& inResource)
{
    // Legacy path: convert a ResourceMesh into a single-submesh V2 binary.
    ResourceMesh* mesh = down_cast<ResourceMesh*>(inResource);

    SubMeshData sub;
    sub.name           = mesh->GetName();
    sub.localTransform = glm::mat4(1.0f);
    sub.vertices       = mesh->vertices;
    sub.indices.assign(mesh->indices.begin(), mesh->indices.end());

    const std::vector<SubMeshData> submeshes = { std::move(sub) };
    const bool ret = SaveSubmeshes(metaFileData, submeshes);
    NOUS_DELETE(mesh, MemoryTag::RESOURCE_MESH);
    return ret;
}

bool ImporterMesh::Deserialize(const std::string& libraryPath, ResourceBase* outResource)
{
    // Merge all submeshes from the binary into a single ResourceMesh (CPU only).
    const auto submeshes = ParseMeshBinary(libraryPath);
    if (submeshes.empty()) return false;

    ResourceMesh* mesh = down_cast<ResourceMesh*>(outResource);
    mesh->vertices.clear();
    mesh->indices.clear();

    for (const auto& sub : submeshes)
    {
        const size_t prevV = mesh->vertices.size();
        mesh->vertices.insert(mesh->vertices.end(), sub.vertices.begin(), sub.vertices.end());

        const size_t prevI = mesh->indices.size();
        mesh->indices.resize(prevI + sub.indices.size());
        for (size_t i = 0; i < sub.indices.size(); ++i)
            mesh->indices[prevI + i] = sub.indices[i] + static_cast<uint32_t>(prevV);
    }

    // Compute local AABB once from the merged vertex set.
    if (!mesh->vertices.empty())
    {
        mesh->localAABBMin = mesh->vertices[0].position;
        mesh->localAABBMax = mesh->vertices[0].position;
        for (const auto& v : mesh->vertices)
        {
            mesh->localAABBMin = glm::min(mesh->localAABBMin, v.position);
            mesh->localAABBMax = glm::max(mesh->localAABBMax, v.position);
        }
    }

    return true;
}

bool ImporterMesh::Upload(ResourceBase* outResource, IGPUResourceFactory* gpu)
{
    ResourceMesh* mesh = down_cast<ResourceMesh*>(outResource);
    return gpu->CreateGeometry(
        mesh->vertices.size(), mesh->vertices.data(),
        mesh->indices.size(), mesh->indices.data(), mesh);
}

void ImporterMesh::Release(ResourceBase* inResource, IGPUResourceFactory* gpu)
{
    ResourceMesh* mesh = down_cast<ResourceMesh*>(inResource);
    if (mesh->internalID != INVALID_ID)
    {
        gpu->DestroyGeometry(mesh);
        mesh->internalID = INVALID_ID;
    }
}

void ImporterMesh::Evict(ResourceBase* inResource)
{
    ResourceMesh* mesh = down_cast<ResourceMesh*>(inResource);
    mesh->vertices.clear();
    mesh->indices.clear();
}

// ─── LoadHierarchy ────────────────────────────────────────────────────────────

std::vector<SubMeshData> ImporterMesh::LoadHierarchy(const std::string& libraryPath)
{
    return ParseMeshBinary(libraryPath);
}
