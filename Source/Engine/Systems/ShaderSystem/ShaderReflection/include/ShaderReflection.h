#ifndef NOUS_ENGINE_SHADER_REFLECTION_H
#define NOUS_ENGINE_SHADER_REFLECTION_H

#include "ShaderReflectionTypes.h"
#include "Engine/Systems/ShaderSystem/ShaderTypes.h"

namespace NOUS_ShaderSystem
{
    ShaderReflectionResult ReflectSpirV(const ShaderSource& source);
}

#endif //NOUS_ENGINE_SHADERREFLECTION_H