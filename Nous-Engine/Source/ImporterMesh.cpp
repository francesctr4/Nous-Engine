#include "ImporterMesh.h"

#include "Assimp.h"
#define ASSIMP_LOAD_FLAGS (aiProcess_Triangulate)

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

void ProcessNode(aiNode* node, const aiScene* scene, Mesh* outMesh)
{
    for (uint32_t i = 0; i < node->mNumMeshes; ++i) {

        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];

        ProcessMesh(mesh, scene, outMesh);

    }

    for (uint32_t i = 0; i < node->mNumChildren; ++i) {

        ProcessNode(node->mChildren[i], scene, outMesh);

    }

}

void ProcessMesh(aiMesh* mesh, const aiScene* scene, Mesh* outMesh)
{
    for (uint32_t i = 0; i < mesh->mNumVertices; ++i) {

        Vertex vertex;

        vertex.position = {

            mesh->mVertices[i].x,
            mesh->mVertices[i].y,
            mesh->mVertices[i].z

        };

        if (mesh->HasTextureCoords(0)) {

            vertex.texCoord = {

                mesh->mTextureCoords[0][i].x,
                mesh->mTextureCoords[0][i].y

            };

        }
        else {

            vertex.texCoord = { 0.0f, 0.0f };

        }

        vertex.color = { 1.0f, 1.0f, 1.0f };

        outMesh->vertices.emplace_back(vertex);

    }

    if (mesh->HasFaces()) {

        for (uint32_t i = 0; i < mesh->mNumFaces; ++i) {

            aiFace face = mesh->mFaces[i];

            for (uint32_t j = 0; j < face.mNumIndices; ++j) {

                outMesh->indices.emplace_back(face.mIndices[j]);

            }

        }

    }

}