#include <Utils/Serialization/JsonArray.h>
#include <Utils/Serialization/JsonObject.h>

#include <parson.h>

// --- Lifecycle ---

JsonArray::JsonArray()
    : m_Value(json_value_init_array())
    , m_Array(json_value_get_array(m_Value))
{}

JsonArray::JsonArray(json_value_t* value)
    : m_Value(value)
    , m_Array(value ? json_value_get_array(value) : nullptr)
{}

JsonArray::JsonArray(JsonArray&& other) noexcept
    : m_Value(other.m_Value)
    , m_Array(other.m_Array)
{
    other.m_Value = nullptr;
    other.m_Array = nullptr;
}

JsonArray& JsonArray::operator=(JsonArray&& other) noexcept {
    if (this != &other) {
        if (m_Value) json_value_free(m_Value);
        m_Value = other.m_Value;
        m_Array = other.m_Array;
        other.m_Value = nullptr;
        other.m_Array = nullptr;
    }
    return *this;
}

JsonArray::~JsonArray() {
    if (m_Value) json_value_free(m_Value);
}

// --- Metadata ---

int JsonArray::Count() const {
    return m_Array ? static_cast<int>(json_array_get_count(m_Array)) : 0;
}

bool JsonArray::IsEmpty() const {
    return m_Value == nullptr;
}

// --- Append ---

void JsonArray::Append(int value) {
    json_array_append_number(m_Array, static_cast<double>(value));
}

void JsonArray::Append(float value) {
    json_array_append_number(m_Array, static_cast<double>(value));
}

void JsonArray::Append(double value) {
    json_array_append_number(m_Array, value);
}

void JsonArray::Append(std::string_view value) {
    json_array_append_string(m_Array, value.data());
}

void JsonArray::Append(JsonObject&& child) {
    json_array_append_value(m_Array, child.m_Value);
    child.m_Value  = nullptr;
    child.m_Object = nullptr;
}

void JsonArray::Append(JsonArray&& child) {
    json_array_append_value(m_Array, child.m_Value);
    child.m_Value = nullptr;
    child.m_Array = nullptr;
}

// --- Get by index ---

int JsonArray::GetInt(int index) const {
    return static_cast<int>(json_array_get_number(m_Array, static_cast<size_t>(index)));
}

float JsonArray::GetFloat(int index) const {
    return static_cast<float>(json_array_get_number(m_Array, static_cast<size_t>(index)));
}

double JsonArray::GetDouble(int index) const {
    return json_array_get_number(m_Array, static_cast<size_t>(index));
}

std::string JsonArray::GetString(int index) const {
    const char* s = json_array_get_string(m_Array, static_cast<size_t>(index));
    return s ? std::string(s) : std::string{};
}

JsonObject JsonArray::GetObject(int index) const {
    JSON_Value* val = json_array_get_value(m_Array, static_cast<size_t>(index));
    if (!val || json_value_get_type(val) != JSONObject) return JsonObject(static_cast<json_value_t*>(nullptr));
    return JsonObject(json_value_deep_copy(val));
}

JsonArray JsonArray::GetArray(int index) const {
    JSON_Value* val = json_array_get_value(m_Array, static_cast<size_t>(index));
    if (!val || json_value_get_type(val) != JSONArray) return JsonArray(static_cast<json_value_t*>(nullptr));
    return JsonArray(json_value_deep_copy(val));
}

// --- Remove ---

void JsonArray::Remove(int index) {
    json_array_remove(m_Array, static_cast<size_t>(index));
}
