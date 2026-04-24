#pragma once

#include "Engine/EngineExport.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <string_view>

class JsonArray;
class JsonFile;

typedef struct json_value_t  json_value_t;
typedef struct json_object_t json_object_t;

class NOUS_ENGINE_API JsonObject {
public:
    JsonObject();
    JsonObject(JsonObject&&) noexcept;
    JsonObject& operator=(JsonObject&&) noexcept;
    ~JsonObject();

    JsonObject(const JsonObject&)            = delete;
    JsonObject& operator=(const JsonObject&) = delete;

    // Write — scalars
    void Set(std::string_view key, int value);
    void Set(std::string_view key, float value);
    void Set(std::string_view key, double value);
    void Set(std::string_view key, bool value);
    void Set(std::string_view key, std::string_view value);

    // Write — GLM (stored as JSON number arrays)
    void Set(std::string_view key, const glm::vec2& value);
    void Set(std::string_view key, const glm::vec3& value);
    void Set(std::string_view key, const glm::vec4& value);
    void Set(std::string_view key, const glm::quat& value);  // [w, x, y, z]

    // Write — children (transfers ownership)
    void Set(std::string_view key, JsonObject&& child);
    void Set(std::string_view key, JsonArray&&  child);

    // Read — scalars (returns def on missing/wrong-type key)
    int         GetInt   (std::string_view key, int         def = 0)    const;
    float       GetFloat (std::string_view key, float       def = 0.f)  const;
    double      GetDouble(std::string_view key, double      def = 0.0)  const;
    bool        GetBool  (std::string_view key, bool        def = false) const;
    std::string GetString(std::string_view key, std::string_view def = "") const;

    // Read — GLM (returns def on missing/wrong-type key)
    glm::vec2 GetVec2(std::string_view key, const glm::vec2& def = {}) const;
    glm::vec3 GetVec3(std::string_view key, const glm::vec3& def = {}) const;
    glm::vec4 GetVec4(std::string_view key, const glm::vec4& def = {}) const;
    glm::quat GetQuat(std::string_view key, const glm::quat& def = {}) const;

    // Read — children (returns owning deep copy; empty if key missing)
    JsonObject GetObject(std::string_view key) const;
    JsonArray  GetArray (std::string_view key) const;

    bool HasKey(std::string_view key) const;
    void Remove(std::string_view key);
    bool IsEmpty() const;

private:
    friend class JsonArray;
    friend class JsonFile;
    explicit JsonObject(json_value_t* value);  // takes ownership

    json_value_t*  m_Value;
    json_object_t* m_Object;
};
