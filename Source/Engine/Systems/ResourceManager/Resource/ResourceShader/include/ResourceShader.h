#ifndef NOUS_ENGINE_RESOURCE_SHADER_H
#define NOUS_ENGINE_RESOURCE_SHADER_H

#include "Engine/Core/Globals.h"
#include "Engine/Systems/ResourceManager/Resource/Resource.h"
#include "Engine/Systems/ShaderSystem/ShaderTypes.h"
#include "Engine/Systems/ShaderSystem/ShaderReflection/include/ShaderReflectionTypes.h"

// No renderer headers included here — forward declaration keeps the
// ResourceManager layer decoupled from any backend implementation.
struct IBackendShader;

class ResourceShader : public Resource
{
public:

    // Constructor & Destructor

    NOUS_ENGINE_API ResourceShader(uint32 uid = 0);
    NOUS_ENGINE_API ~ResourceShader() override;

public:

    uint32 ID;
    uint32 internalID;
    uint32 generation;

    std::vector<ShaderSource> stagesData;
    PipelineReflectionResult reflection;        // merged interface (pipeline-level)

    IBackendShader* internalData = nullptr;     // owned by the active backend (e.g. VulkanShader*)

};

#endif //NOUS_ENGINE_RESOURCE_SHADER_H