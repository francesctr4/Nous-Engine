#ifndef NOUS_ENGINE_IGPU_RESOURCE_FACTORY_H
#define NOUS_ENGINE_IGPU_RESOURCE_FACTORY_H

#include <cstdint>

// Forward declarations
class ResourceMesh;
class ResourceMaterial;
class ResourceTexture;
class ResourceShader;
struct Vertex3D;

/**
 * @brief Narrow GPU resource creation/destruction interface.
 *
 * This is the only GPU capability that ModuleResourceManager and importers
 * depend on. RendererFrontend implements it.
 *
 * Keeping this interface separate ensures the resource system has no
 * dependency on the full rendering pipeline.
 */
class IGPUResourceFactory
{
public:
    virtual ~IGPUResourceFactory() = default;

    [[nodiscard]] virtual bool CreateTexture(const uint8_t* pixels, ResourceTexture* outTexture) = 0;
    virtual void DestroyTexture(ResourceTexture* texture) = 0;

    [[nodiscard]] virtual bool CreateMaterial(ResourceMaterial* material) = 0;
    virtual void DestroyMaterial(ResourceMaterial* material) = 0;

    [[nodiscard]] virtual bool CreateGeometry(uint32_t vertexCount, const Vertex3D* vertices,
                                              uint32_t indexCount, const uint32_t* indices,
                                              ResourceMesh* outGeometry) = 0;
    virtual void DestroyGeometry(ResourceMesh* geometry) = 0;

    [[nodiscard]] virtual bool CreateShader(ResourceShader* shader) = 0;
    virtual void DestroyShader(ResourceShader* shader) = 0;
};

#endif // NOUS_ENGINE_IGPU_RESOURCE_FACTORY_H
