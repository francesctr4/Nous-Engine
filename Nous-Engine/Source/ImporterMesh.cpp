#include "ImporterMesh.h"
#include "FileHandle.h"

#include "Assimp.h"
#define ASSIMP_LOAD_FLAGS (aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_JoinIdenticalVertices | aiProcess_CalcTangentSpace)

void ProcessNode(aiNode* node, const aiScene* scene, Mesh* outMesh);
void ProcessMesh(aiMesh* mesh, const aiScene* scene, Mesh* outMesh);

bool ImporterMesh::Import(const char* path, Mesh* mesh)
{
    bool ret = true;

    const aiScene* scene = aiImportFile(path, ASSIMP_LOAD_FLAGS);

    if (scene != nullptr && scene->HasMeshes())
    {
        ProcessNode(scene->mRootNode, scene, mesh);

        aiReleaseImport(scene);
    }
    else
    {
        NOUS_ERROR("Failed to load 3D Model: %s", path);
        ret = false;
    }

    return ret;
}

bool ImporterMesh::Save(const char* path, const Mesh& mesh)
{
    bool ret = true;

    FileHandle fileHandle;
    if (!fileHandle.Open(path, FileMode::WRITE, true))
    {
        return false; // Failed to open file for writing
    }

    uint64 bytesWritten = 0;

    // ------------------ VERTICES ------------------ //
    
    uint64 vertexCount = mesh.vertices.size();

    // Write vertex count
    if (!fileHandle.Write(sizeof(vertexCount), &vertexCount, &bytesWritten))
    {
        ret = false;
    }

    // Write vertex data
    if (!fileHandle.Write(vertexCount * sizeof(Vertex), mesh.vertices.data(), &bytesWritten))
    {
        ret = false;
    }

    // ------------------ INDICES ------------------ //
    
    uint64 indexCount = mesh.indices.size();

    // Write index count
    if (!fileHandle.Write(sizeof(indexCount), &indexCount, &bytesWritten))
    {
        ret = false;
    }

    // Write index data
    if (!fileHandle.Write(indexCount * sizeof(uint32), mesh.indices.data(), &bytesWritten))
    {
        ret = false;
    }

    fileHandle.Close();

    return ret;
}

bool ImporterMesh::Load(const char* path, Mesh* mesh)
{
    bool ret = true;

    FileHandle fileHandle;
    if (!fileHandle.Open(path, FileMode::READ, true))
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
    if (!fileHandle.ReadBytes(vertexCount * sizeof(Vertex), reinterpret_cast<char*>(mesh->vertices.data()), &bytesRead))
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

    return ret;
}

void ProcessNode(aiNode* node, const aiScene* scene, Mesh* outMesh)
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

void ProcessMesh(aiMesh* mesh, const aiScene* scene, Mesh* outMesh)
{
    // Vertices
    for (uint32 i = 0; i < mesh->mNumVertices; ++i) 
    {
        Vertex vertex;

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

        outMesh->vertices.emplace_back(vertex);
    }

    // Indices
    if (mesh->HasFaces()) 
    {
        for (uint32_t i = 0; i < mesh->mNumFaces; ++i) 
        {
            aiFace face = mesh->mFaces[i];

            for (uint32_t j = 0; j < face.mNumIndices; ++j) 
            {
                outMesh->indices.emplace_back(face.mIndices[j]);
            }
        }
    }

}