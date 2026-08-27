#pragma once

#include "Engine/EngineExport.h"
#include <Utils/Serialization/JsonObject.h>
#include <string_view>

class NOUS_ENGINE_API JsonFile {
public:
    JsonFile()  = delete;
    ~JsonFile() = delete;

    // Returns empty JsonObject on parse failure; logs NOUS_ERROR
    static JsonObject LoadFromFile(std::string_view path);

    // Returns false and logs NOUS_ERROR on write failure
    static bool SaveToFile(const JsonObject& root, std::string_view path);
};
