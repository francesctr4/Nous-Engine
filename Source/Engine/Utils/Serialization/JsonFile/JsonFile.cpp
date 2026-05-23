#include "JsonFile.h"

#include <parson.h>
#include "Engine/Core/Logger/Logger.h"

JsonObject JsonFile::LoadFromFile(std::string_view path) {
    JSON_Value* root = json_parse_file(path.data());
    if (!root) {
        NOUS_ERROR("JsonFile: failed to parse '%s'", path.data());
        return JsonObject(static_cast<JSON_Value*>(nullptr));
    }
    if (json_value_get_type(root) != JSONObject) {
        NOUS_ERROR("JsonFile: root is not an object in '%s'", path.data());
        json_value_free(root);
        return JsonObject(static_cast<JSON_Value*>(nullptr));
    }
    return JsonObject(root);
}

bool JsonFile::SaveToFile(const JsonObject& root, std::string_view path) {
    JSON_Status status = json_serialize_to_file_pretty(root.m_Value, path.data());
    if (status != JSONSuccess) {
        NOUS_ERROR("JsonFile: failed to save '%s'", path.data());
        return false;
    }
    return true;
}
