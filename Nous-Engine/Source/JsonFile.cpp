#include "JsonFile.h"

#include "External/Parson/parson.h"

#include "FileManager.h"

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

// Get integer value
bool JsonFile::GetValue(const char* key, int& value) const
{
    if (json_object_has_value_of_type(rootObject, key, JSONNumber)) 
    {
        value = static_cast<int>(json_object_get_number(rootObject, key));
        return true;
    }
    return false;
}

// Get float value
bool JsonFile::GetValue(const char* key, float& value) const
{
    if (json_object_has_value_of_type(rootObject, key, JSONNumber)) {
        value = static_cast<float>(json_object_get_number(rootObject, key));
        return true;
    }
    return false;
}

// Get double value
bool JsonFile::GetValue(const char* key, double& value) const
{
    if (json_object_has_value_of_type(rootObject, key, JSONNumber)) {
        value = json_object_get_number(rootObject, key);
        return true;
    }
    return false;
}

// Get string value
bool JsonFile::GetValue(const char* key, std::string& value) const
{
    if (json_object_has_value_of_type(rootObject, key, JSONString)) 
    {
        const char* str = json_object_get_string(rootObject, key);
        if (str) 
        {
            value = str;
            return true;
        }
    }
    return false;
}

// Get boolean value
bool JsonFile::GetValue(const char* key, bool& value) const
{
    if (json_object_has_value_of_type(rootObject, key, JSONBoolean)) {
        value = json_object_get_boolean(rootObject, key) != 0;
        return true;
    }
    return false;
}

// Get float4 value (assuming float4 is a structure with x, y, z, w members)
bool JsonFile::GetValue(const char* key, float4& value) const
{
    if (json_object_has_value_of_type(rootObject, key, JSONArray)) {
        JSON_Array* jsonArray = json_object_get_array(rootObject, key);
        if (jsonArray && json_array_get_count(jsonArray) == 4) {
            value.x = static_cast<float>(json_array_get_number(jsonArray, 0));
            value.y = static_cast<float>(json_array_get_number(jsonArray, 1));
            value.z = static_cast<float>(json_array_get_number(jsonArray, 2));
            value.w = static_cast<float>(json_array_get_number(jsonArray, 3));
            return true;
        }
    }
    return false;
}

bool JsonFile::SaveToFile(const char* path)
{
    return (json_serialize_to_file_pretty(rootValue, path) == JSONSuccess);
}

// Load JSON from file
bool JsonFile::LoadFromFile(const char* path)
{
    if (NOUS_FileManager::Exists(path)) 
    {
        JSON_Value* newValue = json_parse_file(path);
        if (newValue) {
            json_value_free(rootValue); // Free the old JSON value
            rootValue = newValue;
            rootObject = json_value_get_object(rootValue);
            return true;
        }
    }
    
    return false;
}