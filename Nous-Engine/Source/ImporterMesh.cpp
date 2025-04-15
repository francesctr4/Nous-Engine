#include "ImporterMesh.h"
#include "FileHandle.h"

#include "ResourceMesh.h"
#include "MetaFileData.inl"

#include "MemoryManager.h"

#include "ModuleRenderer3D.h"
#include "RendererFrontend.h"
#include "ModuleResourceManager.h"
#include "ResourceMaterial.h"
#include "ResourceTexture.h"

#include "Assimp.h"
#define ASSIMP_LOAD_FLAGS (aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_JoinIdenticalVertices | aiProcess_CalcTangentSpace)

void ProcessNode(aiNode* node, const aiScene* scene, Resource*& outMesh);
void ProcessMesh(aiMesh* mesh, const aiScene* scene, Resource*& outMesh);

bool ImporterMesh::Import(const MetaFileData& metaFileData)
{
    Resource* tempMesh = NOUS_NEW<ResourceMesh>(MemoryManager::MemoryTag::RESOURCE_MESH);

    const aiScene* scene = aiImportFile(metaFileData.assetsPath.c_str(), ASSIMP_LOAD_FLAGS);

    if (scene != nullptr && scene->HasMeshes())
    {
        ProcessNode(scene->mRootNode, scene, tempMesh);

        aiReleaseImport(scene);
    }
    else
    {
        NOUS_ERROR("Failed to load 3D Model: %s", metaFileData.assetsPath.c_str());
        return false;
    }

    return Save(metaFileData, tempMesh);
}

bool ImporterMesh::Save(const MetaFileData& metaFileData, Resource*& inResource)
{
    ResourceMesh* mesh = down_cast<ResourceMesh*>(inResource);

    bool ret = true;

    FileHandle fileHandle;
    if (!fileHandle.Open(metaFileData.libraryPath, FileMode::WRITE, true))
    {
        return false; // Failed to open file for writing
    }

    uint64 bytesWritten = 0;

    // ------------------ VERTICES ------------------ //

    uint64 vertexCount = mesh->vertices.size();

    // Write vertex count
    if (!fileHandle.Write(sizeof(vertexCount), &vertexCount, &bytesWritten))
    {
        ret = false;
    }

    // Write vertex data
    if (!fileHandle.Write(vertexCount * sizeof(Vertex3D), mesh->vertices.data(), &bytesWritten))
    {
        ret = false;
    }

    // ------------------ INDICES ------------------ //

    uint64 indexCount = mesh->indices.size();

    // Write index count
    if (!fileHandle.Write(sizeof(indexCount), &indexCount, &bytesWritten))
    {
        ret = false;
    }

    // Write index data
    if (!fileHandle.Write(indexCount * sizeof(uint32), mesh->indices.data(), &bytesWritten))
    {
        ret = false;
    }

    fileHandle.Close();

    NOUS_DELETE<ResourceMesh>(mesh, MemoryManager::MemoryTag::RESOURCE_MESH);

    return ret;
}

bool ImporterMesh::Load(const std::string& libraryPath, Resource* outResource)
{
    ResourceMesh* mesh = down_cast<ResourceMesh*>(outResource);

    bool ret = true;

    FileHandle fileHandle;
    if (!fileHandle.Open(libraryPath, FileMode::READ, true))
    {
        return false;
    }

    uint64 bytesRead = 0;

    // ------------------ VERTICES ------------------ //

    uint64 vertexCount = 0;

    // Read vertex count
    if (!fileHandle.ReadBytes(sizeof(vertexCount), reinterpret_cast<char*>(&vertexCount), &bytesRead))
    {
        ret = false;
    }

    mesh->vertices.resize(vertexCount);

    // Read vertex data
    if (!fileHandle.ReadBytes(vertexCount * sizeof(Vertex3D), reinterpret_cast<char*>(mesh->vertices.data()), &bytesRead))
    {
        ret = false;
    }

    // ------------------ INDICES ------------------ //

    uint64 indexCount = 0;

    // Read index count
    if (!fileHandle.ReadBytes(sizeof(indexCount), reinterpret_cast<char*>(&indexCount), &bytesRead))
    {
        ret = false;
    }

    mesh->indices.resize(indexCount);

    // Read index data
    if (!fileHandle.ReadBytes(indexCount * sizeof(uint32), reinterpret_cast<char*>(mesh->indices.data()), &bytesRead))
    {
        ret = false;
    }

    fileHandle.Close();

    ret = External->renderer->rendererFrontend->CreateGeometry(mesh->vertices.size(), mesh->vertices.data(),
        mesh->indices.size(), mesh->indices.data(), mesh);

    return ret;
}

bool ImporterMesh::Unload(Resource* inResource)
{
    ResourceMesh* mesh = down_cast<ResourceMesh*>(inResource);

    if (mesh->material != nullptr) 
    {
        if (mesh->material->diffuseMap.texture != nullptr)
        {
            External->resourceManager->UnloadResource(mesh->material->diffuseMap.texture->GetUID());
            mesh->material->diffuseMap.texture = nullptr;
        }

        External->resourceManager->UnloadResource(mesh->material->GetUID());
    }

    External->renderer->rendererFrontend->DestroyGeometry(mesh);

    mesh->ID = INVALID_ID;
    mesh->internalID = INVALID_ID;
    mesh->generation = INVALID_ID;

    mesh->vertices.clear();
    mesh->indices.clear();

    return true;
}

void ProcessNode(aiNode* node, const aiScene* scene, Resource*& outMesh)
{
    for (uint32 i = 0; i < node->mNumMeshes; ++i)
    {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        ProcessMesh(mesh, scene, outMesh);
    }

    for (uint32 i = 0; i < node->mNumChildren; ++i)
    {
        ProcessNode(node->mChildren[i], scene, outMesh);
    }
}

void ProcessMesh(aiMesh* mesh, const aiScene* scene, Resource*& outMesh)
{
    // Vertices
    for (uint32 i = 0; i < mesh->mNumVertices; ++i)
    {
        Vertex3D vertex;

        // Position
        vertex.position =
        {
            mesh->mVertices[i].x,
            mesh->mVertices[i].y,
            mesh->mVertices[i].z
        };

        // Color
        if (mesh->HasVertexColors(0))
        {
            vertex.color =
            {
                mesh->mColors[0][i].r,
                mesh->mColors[0][i].g,
                mesh->mColors[0][i].b
            };
        }
        else
        {
            vertex.color = { 1.0f, 1.0f, 1.0f };
        }

        // Texture Coords
        if (mesh->HasTextureCoords(0))
        {
            vertex.texCoord =
            {
                mesh->mTextureCoords[0][i].x,
                mesh->mTextureCoords[0][i].y
            };
        }
        else
        {
            vertex.texCoord = { 0.0f, 0.0f };
        }

        down_cast<ResourceMesh*>(outMesh)->vertices.emplace_back(vertex);
    }

    // Indices
    if (mesh->HasFaces())
    {
        for (uint32_t i = 0; i < mesh->mNumFaces; ++i)
        {
            aiFace face = mesh->mFaces[i];

            for (uint32_t j = 0; j < face.mNumIndices; ++j)
            {
                down_cast<ResourceMesh*>(outMesh)->indices.emplace_back(face.mIndices[j]);
            }
        }
    }
}