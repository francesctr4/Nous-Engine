#ifndef NOUS_ENGINE_RESOURCE_SHADER_H
#define NOUS_ENGINE_RESOURCE_SHADER_H

#include <EngineCore/Globals.h>
#include <ResourceManager/Core/ResourceBase.h>
#include <ShaderSystem/ShaderTypes.h>
#include <ShaderSystem/ShaderReflection/ShaderReflectionTypes.h>

// No renderer headers included here — forward declaration keeps the
// ResourceManager layer decoupled from any backend implementation.
struct IBackendShader;

class ResourceShader : public ResourceBase
{
public:

    // Constructor & Destructor

    NOUS_ENGINE_API ResourceShader(uint32 uid = 0);
    NOUS_ENGINE_API ~ResourceShader() override;

public:

    // Incremented each time the shader is hot-reloaded (GPU swap completed).
    // Synced to backend-owned clones (e.g. builtInGameShader) after each reload
    // so all consumers can detect that their pipeline is stale.
    uint32 generation;

    std::vector<ShaderSource> stagesData;
    PipelineReflectionResult reflection;        // merged interface (pipeline-level)

    // Owning pointer to the backend's GPU resources (e.g. VulkanShader*).
    // Allocated and freed exclusively by the renderer backend.
    // Swapped atomically during hot-reload: old destroyed, new created, pointer updated.
    IBackendShader* internalData = nullptr;

};

#endif //NOUS_ENGINE_RESOURCE_SHADER_H