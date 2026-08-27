#include "JsonFile.h"

#include <parson.h>
#include <FileSystem/FileSystem.h>
#include <Logger/Logger.h>

JsonObject JsonFile::LoadFromFile(std::string_view path) {
    // Normalize separators (Windows-authored .meta/.nous/.nmat paths use backslashes, which are
    // literal filename chars on POSIX) and guarantee null-termination for parson's C API.
    const std::string filePath = nous::engine::filesystem::NormalizePath(std::string(path));
    JSON_Value* root = json_parse_file(filePath.c_str());
    if (!root) {
        NOUS_ERROR("JsonFile: failed to parse '%s'", filePath.c_str());
        return JsonObject(static_cast<JSON_Value*>(nullptr));
    }
    if (json_value_get_type(root) != JSONObject) {
        NOUS_ERROR("JsonFile: root is not an object in '%s'", filePath.c_str());
        json_value_free(root);
        return JsonObject(static_cast<JSON_Value*>(nullptr));
    }
    return JsonObject(root);
}

bool JsonFile::SaveToFile(const JsonObject& root, std::string_view path) {
    const std::string filePath = nous::engine::filesystem::NormalizePath(std::string(path));
    JSON_Status status = json_serialize_to_file_pretty(root.m_Value, filePath.c_str());
    if (status != JSONSuccess) {
        NOUS_ERROR("JsonFile: failed to save '%s'", filePath.c_str());
        return false;
    }
    return true;
}
