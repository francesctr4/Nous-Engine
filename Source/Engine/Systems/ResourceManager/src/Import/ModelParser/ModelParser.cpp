#include <ResourceManager/Import/ModelParser/ModelParser.h>

#include <ResourceManager/Import/ModelParser/ClipBuild.h>
#include <ResourceManager/Import/ModelParser/SkeletonBuild.h>
#include <ResourceManager/Core/IResourceLoader.h>

#include <FileSystem/FileHandle.h>
#include <Logger/Logger.h>

#include <Utils/Serialization/JsonArray.h>
#include <Utils/Serialization/JsonFile.h>
#include <Utils/Serialization/JsonObject.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

// ─── Assimp ───────────────────────────────────────────────────────────────────
//
// THE ONLY TRANSLATION UNIT IN THE ENGINE THAT INCLUDES ASSIMP. Same compile
// firewall as RendererBackendFactory.cpp (Vulkan), AudioBackendFactory.cpp
// (miniaudio) and VideoDecoderBackendFactory.cpp (FFmpeg). If a second .cpp ever
// needs an aiScene, the answer is to widen ModelImportData, not to include this.
//
// aiProcess_LimitBoneWeights is required, not optional: assimp's cap is 4, which
// matches the vertex layout, but WITHOUT the flag an export carrying 6 influences
// silently drops the extras in arbitrary order instead of keeping the 4 heaviest
// and renormalizing. It has no effect on unskinned meshes, so it is safe to have
// been added ahead of the V4 vertex format.
#define ASSIMP_LOAD_FLAGS (aiProcess_Triangulate | aiProcess_GenSmoothNormals | \
                           aiProcess_JoinIdenticalVertices | aiProcess_CalcTangentSpace | \
                           aiProcess_LimitBoneWeights)
#include "assimp/anim.h"
#include "assimp/cimport.h"
#include "assimp/material.h"
#include "assimp/postprocess.h"
#include "assimp/scene.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <format>
#include <map>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace nous::engine::resource_manager
{
    using animation_system::AnimClipData;
    using animation_system::Transform;

    namespace
    {
        // ─── Conversions ──────────────────────────────────────────────────────

        // Assimp is row-major; GLM is column-major. Lifted out of ImporterMesh.cpp
        // so a second copy cannot appear and disagree about that.
        glm::mat4 AiToGlm(const aiMatrix4x4& m)
        {
            return glm::mat4(
                m.a1, m.b1, m.c1, m.d1,   // column 0
                m.a2, m.b2, m.c2, m.d2,   // column 1
                m.a3, m.b3, m.c3, m.d3,   // column 2
                m.a4, m.b4, m.c4, m.d4    // column 3
            );
        }

        glm::vec3 AiToGlm(const aiVector3D& v) { return { v.x, v.y, v.z }; }

        glm::quat AiToGlm(const aiQuaternion& q) { return { q.w, q.x, q.y, q.z }; }

        // A node's local transform as TRS. Used for SkeletonData::bindLocals, which
        // is why the pre-pass walks nodes rather than collecting aiBones: an aiBone
        // carries only an offset matrix, so a bone-only pass would have to invert
        // its way back to the local bind through the parent chain.
        Transform DecomposeToTransform(const aiMatrix4x4& m)
        {
            glm::vec3 skew;
            glm::vec4 perspective;

            Transform out;
            glm::decompose(AiToGlm(m), out.scale, out.rotation, out.position, skew, perspective);

            return out;
        }

        // ─── Submesh extraction (moved verbatim from ImporterMesh.cpp) ────────

        // Weld smooth normals per position for the outline pass. For each vertex,
        // accumulates and averages the face normals of all vertices sharing the
        // same position, then normalizes the result into smoothNormal.
        void WeldSmoothNormals(SubMeshData& out, size_t startIdx)
        {
            struct Vec3Less
            {
                bool operator()(const glm::vec3& a, const glm::vec3& b) const
                {
                    if (a.x != b.x) return a.x < b.x;
                    if (a.y != b.y) return a.y < b.y;
                    return a.z < b.z;
                }
            };

            std::map<glm::vec3, std::pair<glm::vec3, uint32_t>, Vec3Less> accum;

            for (size_t i = startIdx; i < out.vertices.size(); ++i)
            {
                auto& [sum, cnt] = accum[out.vertices[i].position];
                sum += out.vertices[i].normal;
                ++cnt;
            }

            for (size_t i = startIdx; i < out.vertices.size(); ++i)
            {
                const auto& [sum, cnt] = accum[out.vertices[i].position];
                out.vertices[i].smoothNormal = glm::normalize(sum / static_cast<float>(cnt));
            }
        }

        // Stamps up to four skinning influences per vertex.
        //
        // aiBone weights are MESH-LOCAL vertex ids, so they are offset by startIdx
        // to land in the submesh's slice of `out.vertices`. Bone INDICES, by
        // contrast, are looked up in the skeleton -- the whole point of building it
        // first.
        //
        // aiProcess_LimitBoneWeights guarantees at most four influences per vertex
        // and renormalizes them, so slots are filled in encounter order with no
        // sorting. A fifth influence would mean the flag is not doing its job, which
        // is why that case warns instead of silently dropping.
        void ApplyBoneWeights(const aiMesh* mesh, const animation_system::SkeletonData& skeleton,
                              size_t startIdx, const std::string& assetsPath, SubMeshData& out)
        {
            if (mesh->mNumBones == 0 || skeleton.BoneCount() == 0) return;

            bool warnedOverflow = false;
            bool warnedUnknown  = false;

            for (uint32_t b = 0; b < mesh->mNumBones; ++b)
            {
                const aiBone* bone = mesh->mBones[b];
                if (!bone) continue;

                const int boneIndex = skeleton.FindBone(bone->mName.C_Str());

                if (boneIndex < 0)
                {
                    // Pruned by BuildSkeleton, or never in the hierarchy. Skipping
                    // leaves those vertices with one fewer influence rather than
                    // writing an index that would read past the palette.
                    if (!warnedUnknown)
                    {
                        NOUS_WARN("ModelParser: mesh '%s' in '%s' references bone '%s' which is not "
                                  "in the skeleton — its weights are ignored.",
                                  mesh->mName.C_Str(), assetsPath.c_str(), bone->mName.C_Str());
                        warnedUnknown = true;
                    }
                    continue;
                }

                for (uint32_t w = 0; w < bone->mNumWeights; ++w)
                {
                    const aiVertexWeight& weight = bone->mWeights[w];
                    if (weight.mWeight <= 0.0f) continue;

                    const size_t vertexIndex = startIdx + weight.mVertexId;
                    if (vertexIndex >= out.vertices.size()) continue;

                    Vertex3D& vertex = out.vertices[vertexIndex];

                    int slot = -1;
                    for (int s = 0; s < 4; ++s)
                    {
                        if (vertex.boneWeights[s] == 0.0f) { slot = s; break; }
                    }

                    if (slot < 0)
                    {
                        if (!warnedOverflow)
                        {
                            NOUS_WARN("ModelParser: '%s' has vertices with more than 4 bone "
                                      "influences — aiProcess_LimitBoneWeights should have "
                                      "prevented this. Extra influences dropped.",
                                      assetsPath.c_str());
                            warnedOverflow = true;
                        }
                        continue;
                    }

                    vertex.boneIDs[slot]     = static_cast<uint32_t>(boneIndex);
                    vertex.boneWeights[slot] = weight.mWeight;
                }
            }
        }

        // Fill one SubMeshData from an aiMesh. materialPaths is indexed by
        // aiMaterial index; the .nmat path for this aiMesh is looked up via
        // mesh->mMaterialIndex. Pass an empty vector to skip material wiring.
        //
        // An unskinned mesh leaves boneIDs/boneWeights at their zero defaults --
        // one vertex layout for everything, see Vertex.inl.
        void ExtractSubMesh(aiMesh* mesh, const glm::mat4& transform, const std::string& name,
                            const std::vector<std::string>& materialPaths,
                            const animation_system::SkeletonData& skeleton,
                            const std::string& assetsPath, SubMeshData& out)
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

                vertex.smoothNormal = { 0.0f, 0.0f, 0.0f };   // computed below

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
            ApplyBoneWeights(mesh, skeleton, startIdx, assetsPath, out);

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

        // Recursively traverse nodes, accumulating the world transform. Each
        // (node, meshIndex) pair becomes one SubMeshData.
        void CollectSubMeshData(aiNode* node, const aiScene* scene, const glm::mat4& parentTransform,
                                const std::vector<std::string>& materialPaths,
                                const animation_system::SkeletonData& skeleton,
                                const std::string& assetsPath,
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
                ExtractSubMesh(mesh, nodeTransform, name, materialPaths, skeleton, assetsPath, sub);
                outSubmeshes.emplace_back(std::move(sub));
            }

            for (uint32_t i = 0; i < node->mNumChildren; ++i)
            {
                CollectSubMeshData(node->mChildren[i], scene, nodeTransform, materialPaths,
                                   skeleton, assetsPath, outSubmeshes);
            }
        }

        // ─── Skeleton extraction ──────────────────────────────────────────────

        // bone name -> aiBone::mOffsetMatrix, gathered across every mesh.
        //
        // The same bone can appear in several meshes (body + armour on one rig)
        // with the same offset; first occurrence wins, and a later disagreement is
        // worth a warning because it means the meshes were exported against
        // different binds.
        std::unordered_map<std::string, glm::mat4> CollectBoneOffsets(const aiScene* scene,
                                                                      const std::string& assetsPath)
        {
            std::unordered_map<std::string, glm::mat4> offsets;

            for (uint32_t m = 0; m < scene->mNumMeshes; ++m)
            {
                const aiMesh* mesh = scene->mMeshes[m];
                if (!mesh) continue;

                for (uint32_t b = 0; b < mesh->mNumBones; ++b)
                {
                    const aiBone* bone = mesh->mBones[b];
                    if (!bone) continue;

                    std::string name(bone->mName.C_Str());
                    if (name.empty()) continue;

                    const glm::mat4 offset = AiToGlm(bone->mOffsetMatrix);

                    if (auto [it, inserted] = offsets.try_emplace(std::move(name), offset); !inserted)
                    {
                        if (it->second != offset)
                        {
                            NOUS_WARN("ModelParser: bone '%s' in '%s' has different offset matrices "
                                      "across meshes — keeping the first. The meshes were likely "
                                      "exported against different bind poses.",
                                      it->first.c_str(), assetsPath.c_str());
                        }
                    }
                }
            }

            return offsets;
        }

        // Depth-first walk producing the flat array BuildSkeleton expects. DFS
        // emits a parent before all of its children, which IS the topological
        // order BuildSkeleton validates — so there is no sort here and nothing to
        // get wrong in one.
        void CollectRawNodes(const aiNode* node, int parent,
                             const std::unordered_map<std::string, glm::mat4>& boneOffsets,
                             std::vector<RawBoneNode>& out)
        {
            const auto selfIndex = static_cast<int>(out.size());

            RawBoneNode raw;
            raw.name      = node->mName.C_Str();
            raw.parent    = parent;
            raw.localBind = DecomposeToTransform(node->mTransformation);

            if (const auto it = boneOffsets.find(raw.name); it != boneOffsets.end())
            {
                raw.isBone = true;
                raw.offset = it->second;
            }

            out.push_back(std::move(raw));

            for (uint32_t i = 0; i < node->mNumChildren; ++i)
                CollectRawNodes(node->mChildren[i], selfIndex, boneOffsets, out);
        }

        // ─── Clip extraction ──────────────────────────────────────────────────

        RawClip ToRawClip(const aiAnimation* animation)
        {
            RawClip raw;
            raw.name           = animation->mName.length > 0
                ? std::string(animation->mName.C_Str())
                : std::string("Animation");
            raw.durationTicks  = animation->mDuration;
            raw.ticksPerSecond = animation->mTicksPerSecond;

            raw.channels.reserve(animation->mNumChannels);

            for (uint32_t c = 0; c < animation->mNumChannels; ++c)
            {
                const aiNodeAnim* nodeAnim = animation->mChannels[c];
                if (!nodeAnim) continue;

                RawChannel channel;
                channel.boneName = nodeAnim->mNodeName.C_Str();

                channel.posTimes.reserve(nodeAnim->mNumPositionKeys);
                channel.posValues.reserve(nodeAnim->mNumPositionKeys);
                for (uint32_t k = 0; k < nodeAnim->mNumPositionKeys; ++k)
                {
                    channel.posTimes.push_back(nodeAnim->mPositionKeys[k].mTime);
                    channel.posValues.push_back(AiToGlm(nodeAnim->mPositionKeys[k].mValue));
                }

                channel.rotTimes.reserve(nodeAnim->mNumRotationKeys);
                channel.rotValues.reserve(nodeAnim->mNumRotationKeys);
                for (uint32_t k = 0; k < nodeAnim->mNumRotationKeys; ++k)
                {
                    channel.rotTimes.push_back(nodeAnim->mRotationKeys[k].mTime);
                    channel.rotValues.push_back(AiToGlm(nodeAnim->mRotationKeys[k].mValue));
                }

                channel.scaleTimes.reserve(nodeAnim->mNumScalingKeys);
                channel.scaleValues.reserve(nodeAnim->mNumScalingKeys);
                for (uint32_t k = 0; k < nodeAnim->mNumScalingKeys; ++k)
                {
                    channel.scaleTimes.push_back(nodeAnim->mScalingKeys[k].mTime);
                    channel.scaleValues.push_back(AiToGlm(nodeAnim->mScalingKeys[k].mValue));
                }

                raw.channels.push_back(std::move(channel));
            }

            return raw;
        }

        std::vector<AnimClipData> ExtractClips(const aiScene* scene, const std::string& assetsPath)
        {
            std::vector<AnimClipData> clips;
            clips.reserve(scene->mNumAnimations);

            for (uint32_t a = 0; a < scene->mNumAnimations; ++a)
            {
                const aiAnimation* animation = scene->mAnimations[a];
                if (!animation) continue;

                size_t dropped = 0;
                AnimClipData clip = BuildClip(ToRawClip(animation), &dropped);

                if (dropped > 0)
                {
                    NOUS_WARN("ModelParser: clip '%s' in '%s' had %zu malformed channel(s) "
                              "(key times and values of different lengths) — dropped.",
                              clip.name.c_str(), assetsPath.c_str(), dropped);
                }

                clips.push_back(std::move(clip));
            }

            return clips;
        }

        // ─── Material fan-out (moved verbatim from ImporterMesh.cpp) ──────────

        // Returns nullptr for slots the shader does not consume — those textures
        // are still imported (so they appear in AssetsBrowser) but are not wired
        // into the .nmat.
        const char* AssimpTypeToSamplerName(aiTextureType type)
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

        // Replace characters that would be awkward in a filename. Keeps alnum,
        // dash, dot, underscore. Everything else becomes '_'.
        std::string SanitizeFilenamePart(const std::string& in)
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

        // Walk every material's texture slots, import each referenced image, and
        // emit a sibling .nmat per material wiring those textures into
        // ForwardBlinnPhong sampler slots. Two texture-reference flavors handled:
        //   - External path (typical for .gltf/.fbx/.obj): resolve relative to the
        //     model's directory; if the sibling file exists, hand it to ImportFile
        //     (idempotent).
        //   - Embedded "*N" reference (typical for .glb): pull the bytes out of
        //     scene->mTextures[N] and write them next to the model with a generated
        //     name, then ImportFile that. Uncompressed (raw RGBA) embeds are rare
        //     and would need stb_image_write to round-trip to PNG — skipped here
        //     with a warning.
        // Existing .nmat files are never overwritten — re-importing the model
        // preserves any tweaks the user made in the Inspector.
        // Returns a vector indexed by aiMaterial index, where each entry is the
        // Assets/-relative path of the generated .nmat (or empty if no material
        // file was emitted). The caller stamps per-submesh materialAssetPath via
        // aiMesh::mMaterialIndex.
        std::vector<std::string> ExtractTexturesAndMaterials(const aiScene* scene,
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

                // First sampler→asset_path wins (multiple assimp types can map to
                // the same shader sampler; e.g. BASE_COLOR + DIFFUSE both →
                // diffuseSampler).
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
                                NOUS_WARN("ModelParser: skipping uncompressed embedded texture in '%s' (slot %d/%d) — RGBA-to-PNG conversion not implemented.",
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
                                    NOUS_WARN("ModelParser: failed to write embedded texture '%s'.", finalAssetPath.c_str());
                                    continue;
                                }
                                fh.Write(std::span(reinterpret_cast<const std::byte*>(aiTex->pcData), aiTex->mWidth));
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
                                NOUS_WARN("ModelParser: referenced texture not found, skipping: '%s'.", finalAssetPath.c_str());
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

                // Resolve a per-material filename. Assimp materials are not
                // guaranteed to have unique names within a scene, so we
                // disambiguate with the index.
                aiString matNameAi;
                mat->Get(AI_MATKEY_NAME, matNameAi);
                std::string matName = (matNameAi.length > 0) ? std::string(matNameAi.C_Str())
                                                             : ("Material_" + std::to_string(m));
                std::string filenameStem = modelStem + "_" + SanitizeFilenamePart(matName);
                if (!usedMatFilenames.insert(filenameStem).second)
                    filenameStem += "_" + std::to_string(m);

                const std::string matAssetPath = (modelDir / (filenameStem + ".nmat")).generic_string();

                // Always record the path so submeshes can resolve to this material,
                // even if we don't (re)write the .nmat below — e.g. a pre-existing
                // user-edited file.
                materialPaths[m] = matAssetPath;

                // Skip — never overwrite a user-edited material.
                if (fs::exists(matAssetPath)) continue;

                // Build defaults matching ForwardBlinnPhong's InstanceUBO layout
                // (mirrors ImporterMaterial::CreateNewMaterialFile so the Inspector
                // sees the same defaults the user would get from "Create Material").
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
                    NOUS_WARN("ModelParser: failed to write generated material '%s'.", matAssetPath.c_str());
                    continue;
                }

                rm->ImportFile(matAssetPath);
                NOUS_INFO("ModelParser: generated material '%s' (%zu texture slot(s)).",
                          matAssetPath.c_str(), samplerToAssetPath.size());
            }

            return materialPaths;
        }

        bool IsGltf(const std::string& assetsPath)
        {
            std::string ext = std::filesystem::path(assetsPath).extension().string();
            std::ranges::transform(ext, ext.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            return ext == ".gltf" || ext == ".glb";
        }
    }

    std::expected<ModelImportData, std::string> ParseModel(const std::string& assetsPath,
                                                           IResourceLoader* resources)
    {
        const aiScene* scene = aiImportFile(assetsPath.c_str(), ASSIMP_LOAD_FLAGS);

        if (!scene)
        {
            const char* reason = aiGetErrorString();
            return std::unexpected(std::format("assimp failed to load '{}': {}",
                                               assetsPath, reason ? reason : "unknown error"));
        }

        // Every early return past this point must release the scene, so the body is
        // written to fall through to a single aiReleaseImport at the end.
        ModelImportData data;
        std::string     error;

        do
        {
            // Gated to .gltf/.glb only: glTF has a well-defined PBR material spec so
            // assimp's reported texture slots are reliable. FBX/OBJ/DAE slot mapping
            // is inconsistent across DCC tools and tends to spam wrong slots, so
            // submeshes from those formats fall back to the default material at
            // spawn time. (Inherited from ImporterMesh; unchanged by the move.)
            if (IsGltf(assetsPath))
                data.materialPaths = ExtractTexturesAndMaterials(scene, assetsPath, resources);

            // SKELETON BEFORE SUBMESHES, and the order is load-bearing: per-vertex
            // boneIDs must index the SkeletonData array, so the skeleton has to
            // exist before any vertex can be stamped with an index into it.
            //
            // This is the payoff of the pre-pass. The spec devotes a section to
            // "canonical bone ordering — critical", because with separate importers
            // the mesh side sees only bones carrying weights while the skeleton side
            // walks every joint, the two orderings disagree, and fingers animate the
            // spine. One function deriving both cannot disagree with itself.
            if (const auto boneOffsets = CollectBoneOffsets(scene, assetsPath); !boneOffsets.empty())
            {
                std::vector<RawBoneNode> nodes;
                CollectRawNodes(scene->mRootNode, -1, boneOffsets, nodes);

                auto skeleton = BuildSkeleton(nodes);
                if (!skeleton)
                {
                    error = std::format("'{}': {}", assetsPath, skeleton.error());
                    break;
                }

                data.skeleton = std::move(*skeleton);

                // Every bone named by a mesh should have matched a node. A miss
                // means the rig references a joint that is not in the hierarchy,
                // which produces a mesh bound to bones that will never animate.
                if (data.skeleton.BoneCount() < boneOffsets.size())
                {
                    NOUS_WARN("ModelParser: '%s' declares %zu bone(s) but only %zu matched a node "
                              "in the hierarchy — the rest cannot be animated.",
                              assetsPath.c_str(), boneOffsets.size(), data.skeleton.BoneCount());
                }
            }

            if (scene->HasMeshes())
                CollectSubMeshData(scene->mRootNode, scene, glm::mat4(1.0f), data.materialPaths,
                                   data.skeleton, assetsPath, data.submeshes);

            // Clips are independent of the skeleton: a node-animated prop has clips
            // and no bones at all.
            if (scene->mNumAnimations > 0)
                data.clips = ExtractClips(scene, assetsPath);

            if (data.submeshes.empty() && !data.HasSkeleton() && data.clips.empty())
                error = std::format("'{}' contains no meshes, skeleton or animations", assetsPath);

        } while (false);

        aiReleaseImport(scene);

        if (!error.empty()) return std::unexpected(std::move(error));

        return data;
    }
}
