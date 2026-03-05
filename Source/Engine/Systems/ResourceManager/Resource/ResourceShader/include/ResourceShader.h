#ifndef NOUS_ENGINE_RESOURCE_SHADER_H
#define NOUS_ENGINE_RESOURCE_SHADER_H

#include "Engine/Core/Globals.h"
#include "Engine/Systems/ResourceManager/Resource/Resource.h"
#include "Engine/Systems/ShaderSystem/ShaderTypes.h"
#include "Engine/Systems/ShaderSystem/ShaderReflection/include/ShaderReflectionTypes.h"

class ResourceShader : public Resource
{
public:

    // Constructor & Destructor

    ResourceShader(UID uid = 0);
    ~ResourceShader() override;

public:

    uint32 ID;
    uint32 internalID;
    uint32 generation;

    std::vector<ShaderSource> stagesData;
    PipelineReflectionResult reflection;     // merged interface (pipeline-level)
    // uint64_t interfaceHash = 0;
    //IBackendShader* internalData = nullptr; // e.g. VulkanBackendShader*

};

#endif //NOUS_ENGINE_RESOURCE_SHADER_H