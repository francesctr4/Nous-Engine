#pragma once

#include "Engine/Systems/ResourceManager/Resource/Resource.h"

#include <utility>
#include <vector>

class ResourceTexture;
class ResourceMaterial;
class IGPUResourceFactory;

// Owns the engine's built-in fallback resources (textures + default material).
// Extracted from ModuleResourceManager — all creation and destruction logic lives here.
//
// Lifecycle:
//   Create()  — called once from ModuleResourceManager::Start().
//               Returns (type, ptr) pairs ready to push into the pending-upload queue.
//               Textures are ordered before the material so they are GPU_READY when
//               the material's descriptor set is first written.
//   Destroy() — called from ModuleResourceManager::ClearResources().
class BuiltinResources
{
public:
    std::vector<std::pair<ResourceType, Resource*>> Create();
    void Destroy(IGPUResourceFactory* gpu);

    [[nodiscard]] ResourceTexture*  GetDefaultTexture()    const;
    [[nodiscard]] ResourceTexture*  GetWhiteTexture()      const;
    [[nodiscard]] ResourceTexture*  GetBlackTexture()      const;
    [[nodiscard]] ResourceTexture*  GetFlatNormalTexture() const;
    [[nodiscard]] ResourceMaterial* GetDefaultMaterial()   const;

private:
    static void DestroyTexture(ResourceTexture*& tex, IGPUResourceFactory* gpu);

    ResourceTexture*  m_defaultTexture    = nullptr;
    ResourceTexture*  m_whiteTexture      = nullptr;
    ResourceTexture*  m_blackTexture      = nullptr;
    ResourceTexture*  m_flatNormalTexture = nullptr;
    ResourceMaterial* m_defaultMaterial   = nullptr;
};
