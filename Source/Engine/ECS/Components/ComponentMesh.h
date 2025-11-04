#ifndef NOUS_ENGINE_COMPONENTMESH_H
#define NOUS_ENGINE_COMPONENTMESH_H

#include <Engine/Systems/Resource Manager/Resource Types/ResourceMesh.h>
#include "Engine/Modules/ModuleResourceManager/include/ModuleResourceManager.h"
#include <Engine/ECS/Component.h>

class CMesh : public Component {
public:
    COMPONENT_TYPE(CMesh)

    ResourceMesh* mesh;

    // ---------- JSON Serialization ----------
    JSON_Value* Serialize() const override;

    void Deserialize(JSON_Object* obj) override;

    void OnDestroy() override;
};

#endif // NOUS_ENGINE_COMPONENTMESH_H
