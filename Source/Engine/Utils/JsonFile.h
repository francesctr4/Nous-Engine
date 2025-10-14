#ifndef JSONFILE_H
#define JSONFILE_H

#include <Engine/Core/Globals.h>
#include <Engine/Utils/MathUtils.h>

#include <glm/glm.hpp>

#include <string>

typedef struct json_value_t  JSON_Value;
typedef struct json_object_t JSON_Object;

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
    void AppendValue(const char* key, const glm::vec4& value) const;

    bool GetValue(const char* key, int& value) const;
    bool GetValue(const char* key, float& value) const;
    bool GetValue(const char* key, double& value) const;
    bool GetValue(const char* key, std::string& value) const;
    bool GetValue(const char* key, bool& value) const;
    bool GetValue(const char* key, glm::vec4& value) const;

    bool SaveToFile(const char* path);
    bool LoadFromFile(const char* path);

private:
    JSON_Value* rootValue;    // Root JSON value
    JSON_Object* rootObject;  // Root JSON object
};

#endif // JSONFILE_H