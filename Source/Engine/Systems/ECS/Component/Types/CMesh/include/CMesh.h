#pragma once

#include "Engine/Systems/ECS/Component/Component.h"
#include "Engine/EngineExport.h"

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
    NOUS_ENGINE_API JsonObject Serialize() const override;

    NOUS_ENGINE_API void Deserialize(const JsonObject& obj) override;

    NOUS_ENGINE_API void OnDestroy() override;
};
