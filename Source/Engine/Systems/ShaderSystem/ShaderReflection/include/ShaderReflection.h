#ifndef NOUS_ENGINE_SHADER_REFLECTION_H
#define NOUS_ENGINE_SHADER_REFLECTION_H

#include "ShaderReflectionTypes.h"
#include "Engine/Systems/ShaderSystem/ShaderTypes.h"

namespace NOUS_ShaderSystem
{
    ShaderReflectionResult ReflectSpirV(const ShaderSource& source);

    // ShaderReflection.h — añadir
    PipelineReflectionResult MergeReflections(
        const std::vector<ShaderReflectionResult>& stages);
}

#endif //NOUS_ENGINE_SHADER_REFLECTION_H