#ifndef NOUS_ENGINE_CMESH_H
#define NOUS_ENGINE_CMESH_H

#include "Engine/Systems/ECS/Component/Component.h"

class ResourceMesh;

class CMesh : public Component {
public:
    COMPONENT_TYPE(CMesh)

    ResourceMesh* mesh;

    // ---------- JSON Serialization ----------
    JSON_Value* Serialize() const override;

    void Deserialize(JSON_Object* obj) override;

    void OnDestroy() override;
};

#endif // NOUS_ENGINE_CMESH_H
