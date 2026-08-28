#pragma once

#include <EngineCore/EngineExport.h>
#include <string>
#include <string_view>

class JsonObject;
class JsonFile;

typedef struct json_value_t json_value_t;
typedef struct json_array_t json_array_t;

class NOUS_ENGINE_API JsonArray {
public:
    JsonArray();
    JsonArray(JsonArray&&) noexcept;
    JsonArray& operator=(JsonArray&&) noexcept;
    ~JsonArray();

    JsonArray(const JsonArray&)            = delete;
    JsonArray& operator=(const JsonArray&) = delete;

    int  Count()   const;
    bool IsEmpty() const;

    // Append
    void Append(int value);
    void Append(float value);
    void Append(double value);
    void Append(std::string_view value);
    void Append(JsonObject&& child);
    void Append(JsonArray&&  child);

    // Get by index (returns default/empty on out-of-range or wrong type)
    int         GetInt   (int index) const;
    float       GetFloat (int index) const;
    double      GetDouble(int index) const;
    std::string GetString(int index) const;
    JsonObject  GetObject(int index) const;  // owning deep copy
    JsonArray   GetArray (int index) const;  // owning deep copy

    void Remove(int index);

private:
    friend class JsonObject;
    friend class JsonFile;
    explicit JsonArray(json_value_t* value);  // takes ownership

    json_value_t* m_Value;
    json_array_t* m_Array;
};
