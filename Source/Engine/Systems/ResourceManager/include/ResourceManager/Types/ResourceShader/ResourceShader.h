#ifndef NOUS_ENGINE_RESOURCE_SHADER_H
#define NOUS_ENGINE_RESOURCE_SHADER_H

#include <ResourceManager/Core/ResourceBase.h>
#include <ShaderSystem/ShaderTypes.h>
#include <ShaderSystem/ShaderReflection/ShaderReflectionTypes.h>
#include <cstdint>

// No renderer headers included here — forward declaration keeps the
// ResourceManager layer decoupled from any backend implementation.
struct IBackendShader;

class ResourceShader : public ResourceBase
{
public:

    // Constructor & Destructor

    NOUS_ENGINE_API ResourceShader(uint32_t uid = 0);
    NOUS_ENGINE_API ~ResourceShader() override;

public:

    // Incremented each time the shader is hot-reloaded (GPU swap completed), so any
    // consumer holding derived GPU state can detect that it is stale. There are no
    // backend-owned shader clones left to keep this in sync with -- the material and
    // background shaders each draw both viewports from a single ResourceShader.
    uint32_t generation;

    std::vector<ShaderSource> stagesData;
    PipelineReflectionResult reflection;        // merged interface (pipeline-level)

    // Owning pointer to the backend's GPU resources (e.g. VulkanShader*).
    // Allocated and freed exclusively by the renderer backend.
    // Swapped atomically during hot-reload: old destroyed, new created, pointer updated.
    IBackendShader* internalData = nullptr;

};

#endif //NOUS_ENGINE_RESOURCE_SHADER_H