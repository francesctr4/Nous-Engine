#ifndef NOUS_ENGINE_CMESH_H
#define NOUS_ENGINE_CMESH_H

#include "Engine/Systems/ECS/Component/Component.h"

#include <cstdint>

class ResourceMesh;

class CMesh : public Component {
public:
    COMPONENT_TYPE(CMesh)

    ResourceMesh* mesh = nullptr;

    // Index into the source asset's submesh list.
    // -1  → load the whole asset as a single merged mesh (legacy / single-mesh path).
    // ≥ 0 → load only that specific submesh via RequestOrCreateSubMeshResource().
    int32_t submeshIndex = -1;

    // ---------- JSON Serialization ----------
    JSON_Value* Serialize() const override;

    void Deserialize(JSON_Object* obj) override;

    void OnDestroy() override;
};

#endif // NOUS_ENGINE_CMESH_H
