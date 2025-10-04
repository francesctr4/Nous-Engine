//
// Created by TheFr on 03/10/2025.
//

#ifndef NOUS_ENGINE_COMPONENTMESH_H
#define NOUS_ENGINE_COMPONENTMESH_H

#include "Systems/Resource Manager/Resource Types/ResourceMesh.h"
#include "Modules/ModuleResourceManager.h"

struct CMesh {
    ResourceMesh* mesh = nullptr;

    ~CMesh() {
        //External->resourceManager->UnloadResource(mesh->GetUID());
    }
};


#endif //NOUS_ENGINE_COMPONENTMESH_H
