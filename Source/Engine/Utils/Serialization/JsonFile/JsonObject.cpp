#include "JsonObject.h"
#include "JsonArray.h"

#include <parson.h>

// --- Lifecycle ---

JsonObject::JsonObject()
    : m_Value(json_value_init_object())
    , m_Object(json_value_get_object(m_Value))
{}

JsonObject::JsonObject(json_value_t* value)
    : m_Value(value)
    , m_Object(value ? json_value_get_object(value) : nullptr)
{}

JsonObject::JsonObject(JsonObject&& other) noexcept
    : m_Value(other.m_Value)
    , m_Object(other.m_Object)
{
    other.m_Value  = nullptr;
    other.m_Object = nullptr;
}

JsonObject& JsonObject::operator=(JsonObject&& other) noexcept {
    if (this != &other) {
        if (m_Value) json_value_free(m_Value);
        m_Value  = other.m_Value;
        m_Object = other.m_Object;
        other.m_Value  = nullptr;
        other.m_Object = nullptr;
    }
    return *this;
}

JsonObject::~JsonObject() {
    if (m_Value) json_value_free(m_Value);
}

// --- Write: scalars ---

void JsonObject::Set(std::string_view key, int value) {
    json_object_set_number(m_Object, key.data(), static_cast<double>(value));
}

void JsonObject::Set(std::string_view key, float value) {
    json_object_set_number(m_Object, key.data(), static_cast<double>(value));
}

void JsonObject::Set(std::string_view key, double value) {
    json_object_set_number(m_Object, key.data(), value);
}

void JsonObject::Set(std::string_view key, bool value) {
    json_object_set_boolean(m_Object, key.data(), value ? 1 : 0);
}

void JsonObject::Set(std::string_view key, std::string_view value) {
    json_object_set_string(m_Object, key.data(), value.data());
}

// --- Write: GLM ---

void JsonObject::Set(std::string_view key, const glm::vec2& value) {
    JSON_Value* arrVal = json_value_init_array();
    JSON_Array* arr    = json_value_get_array(arrVal);
    json_array_append_number(arr, value.x);
    json_array_append_number(arr, value.y);
    json_object_set_value(m_Object, key.data(), arrVal);
}

void JsonObject::Set(std::string_view key, const glm::vec3& value) {
    JSON_Value* arrVal = json_value_init_array();
    JSON_Array* arr    = json_value_get_array(arrVal);
    json_array_append_number(arr, value.x);
    json_array_append_number(arr, value.y);
    json_array_append_number(arr, value.z);
    json_object_set_value(m_Object, key.data(), arrVal);
}

void JsonObject::Set(std::string_view key, const glm::vec4& value) {
    JSON_Value* arrVal = json_value_init_array();
    JSON_Array* arr    = json_value_get_array(arrVal);
    json_array_append_number(arr, value.x);
    json_array_append_number(arr, value.y);
    json_array_append_number(arr, value.z);
    json_array_append_number(arr, value.w);
    json_object_set_value(m_Object, key.data(), arrVal);
}

void JsonObject::Set(std::string_view key, const glm::quat& value) {
    JSON_Value* arrVal = json_value_init_array();
    JSON_Array* arr    = json_value_get_array(arrVal);
    json_array_append_number(arr, value.w);
    json_array_append_number(arr, value.x);
    json_array_append_number(arr, value.y);
    json_array_append_number(arr, value.z);
    json_object_set_value(m_Object, key.data(), arrVal);
}

// --- Write: children ---

void JsonObject::Set(std::string_view key, JsonObject&& child) {
    json_object_set_value(m_Object, key.data(), child.m_Value);
    child.m_Value  = nullptr;
    child.m_Object = nullptr;
}

void JsonObject::Set(std::string_view key, JsonArray&& child) {
    json_object_set_value(m_Object, key.data(), child.m_Value);
    child.m_Value = nullptr;
    child.m_Array = nullptr;
}

// --- Read: scalars ---

int JsonObject::GetInt(std::string_view key, int def) const {
    JSON_Value* val = json_object_get_value(m_Object, key.data());
    if (!val || json_value_get_type(val) != JSONNumber) return def;
    return static_cast<int>(json_value_get_number(val));
}

float JsonObject::GetFloat(std::string_view key, float def) const {
    JSON_Value* val = json_object_get_value(m_Object, key.data());
    if (!val || json_value_get_type(val) != JSONNumber) return def;
    return static_cast<float>(json_value_get_number(val));
}

double JsonObject::GetDouble(std::string_view key, double def) const {
    JSON_Value* val = json_object_get_value(m_Object, key.data());
    if (!val || json_value_get_type(val) != JSONNumber) return def;
    return json_value_get_number(val);
}

bool JsonObject::GetBool(std::string_view key, bool def) const {
    JSON_Value* val = json_object_get_value(m_Object, key.data());
    if (!val || json_value_get_type(val) != JSONBoolean) return def;
    return json_value_get_boolean(val) != 0;
}

std::string JsonObject::GetString(std::string_view key, std::string_view def) const {
    const char* s = json_object_get_string(m_Object, key.data());
    return s ? std::string(s) : std::string(def);
}

// --- Read: GLM ---

glm::vec2 JsonObject::GetVec2(std::string_view key, const glm::vec2& def) const {
    JSON_Array* arr = json_object_get_array(m_Object, key.data());
    if (!arr || json_array_get_count(arr) < 2) return def;
    return glm::vec2{
        static_cast<float>(json_array_get_number(arr, 0)),
        static_cast<float>(json_array_get_number(arr, 1))
    };
}

glm::vec3 JsonObject::GetVec3(std::string_view key, const glm::vec3& def) const {
    JSON_Array* arr = json_object_get_array(m_Object, key.data());
    if (!arr || json_array_get_count(arr) < 3) return def;
    return glm::vec3{
        static_cast<float>(json_array_get_number(arr, 0)),
        static_cast<float>(json_array_get_number(arr, 1)),
        static_cast<float>(json_array_get_number(arr, 2))
    };
}

glm::vec4 JsonObject::GetVec4(std::string_view key, const glm::vec4& def) const {
    JSON_Array* arr = json_object_get_array(m_Object, key.data());
    if (!arr || json_array_get_count(arr) < 4) return def;
    return glm::vec4{
        static_cast<float>(json_array_get_number(arr, 0)),
        static_cast<float>(json_array_get_number(arr, 1)),
        static_cast<float>(json_array_get_number(arr, 2)),
        static_cast<float>(json_array_get_number(arr, 3))
    };
}

glm::quat JsonObject::GetQuat(std::string_view key, const glm::quat& def) const {
    JSON_Array* arr = json_object_get_array(m_Object, key.data());
    if (!arr || json_array_get_count(arr) < 4) return def;
    // Stored as [w, x, y, z]
    return glm::quat{
        static_cast<float>(json_array_get_number(arr, 0)),  // w
        static_cast<float>(json_array_get_number(arr, 1)),  // x
        static_cast<float>(json_array_get_number(arr, 2)),  // y
        static_cast<float>(json_array_get_number(arr, 3))   // z
    };
}

// --- Read: children ---

JsonObject JsonObject::GetObject(std::string_view key) const {
    JSON_Value* val = json_object_get_value(m_Object, key.data());
    if (!val || json_value_get_type(val) != JSONObject) return JsonObject(static_cast<json_value_t*>(nullptr));
    return JsonObject(json_value_deep_copy(val));
}

JsonArray JsonObject::GetArray(std::string_view key) const {
    JSON_Value* val = json_object_get_value(m_Object, key.data());
    if (!val || json_value_get_type(val) != JSONArray) return JsonArray(static_cast<json_value_t*>(nullptr));
    return JsonArray(json_value_deep_copy(val));
}

// --- Utilities ---

bool JsonObject::HasKey(std::string_view key) const {
    return json_object_get_value(m_Object, key.data()) != nullptr;
}

void JsonObject::Remove(std::string_view key) {
    json_object_remove(m_Object, key.data());
}

bool JsonObject::IsEmpty() const {
    return m_Value == nullptr;
}
