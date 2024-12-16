#pragma once

#include "Globals.h"
#include "External/Parson/parson.h"
#include "MathUtils.h"

class JsonFile 
{
public:

    JsonFile();
    ~JsonFile();

    void AppendValue(const char* key, const int& value) const;
    void AppendValue(const char* key, const float& value) const;
    void AppendValue(const char* key, const double& value) const;
    void AppendValue(const char* key, const std::string& value) const;
    void AppendValue(const char* key, const char* value) const;
    void AppendValue(const char* key, const bool& value) const;
    void AppendValue(const char* key, const float4& value) const;

    bool SaveToFile(const char* path);
    //bool LoadFromFile(const char* path);

private:
    JSON_Value* rootValue;    // Root JSON value
    JSON_Object* rootObject;  // Root JSON object
};