#include "JsonFile.h"

JsonFile::JsonFile()
{
	rootValue = json_value_init_object();
	rootObject = json_value_get_object(rootValue);
}

JsonFile::~JsonFile()
{
	json_value_free(rootValue); // Clean up
}


void JsonFile::AppendValue(const char* key, const int& value) const
{
    json_object_set_number(rootObject, key, static_cast<double>(value));
}

void JsonFile::AppendValue(const char* key, const float& value) const
{
    json_object_set_number(rootObject, key, static_cast<double>(value));
}

void JsonFile::AppendValue(const char* key, const double& value) const
{
    json_object_set_number(rootObject, key, value);
}

void JsonFile::AppendValue(const char* key, const std::string& value) const
{
    json_object_set_string(rootObject, key, value.c_str());
}

void JsonFile::AppendValue(const char* key, const char* value) const
{
    json_object_set_string(rootObject, key, value);
}

void JsonFile::AppendValue(const char* key, const bool& value) const
{
    json_object_set_boolean(rootObject, key, value);
}

void JsonFile::AppendValue(const char* key, const float4& value) const
{
    JSON_Value* jsonArrayValue = json_value_init_array();
    JSON_Array* jsonArrayObject = json_value_get_array(jsonArrayValue);

    // Append the individual components of the glm::vec4 to the array
    json_array_append_number(jsonArrayObject, value.x);
    json_array_append_number(jsonArrayObject, value.y);
    json_array_append_number(jsonArrayObject, value.z);
    json_array_append_number(jsonArrayObject, value.w);

    json_object_set_value(rootObject, key, jsonArrayValue);
}

bool JsonFile::SaveToFile(const char* path)
{
    return (json_serialize_to_file_pretty(rootValue, path) == JSONSuccess);
}
