#ifndef NOUS_ENGINE_SHADER_REFLECTION_SERIALIZER_H
#define NOUS_ENGINE_SHADER_REFLECTION_SERIALIZER_H

#include <string>
#include <ShaderSystem/ShaderReflection/ShaderReflectionTypes.h>

namespace nous::engine::shader_system
{
    // Serializes a merged PipelineReflectionResult to a JSON file.
    bool SerializeReflection(const PipelineReflectionResult& reflection,
                             const std::string& jsonPath);

    // Deserializes a PipelineReflectionResult from a previously saved JSON file.
    // Returns false if the file does not exist or is malformed.
    bool DeserializeReflection(const std::string& jsonPath,
                               PipelineReflectionResult& outReflection);
}

#endif // NOUS_ENGINE_SHADER_REFLECTION_SERIALIZER_H
