#include "Engine/Systems/ECS/Component/CPrefab/include/CPrefab.h"

#include <parson.h>

JSON_Value* CPrefab::Serialize() const
{
    JSON_Value* val = json_value_init_object();
    JSON_Object* obj = json_value_get_object(val);
    json_object_set_string(obj, "type", GetType().c_str());
    json_object_set_string(obj, "prefabSourcePath", prefabSourcePath.c_str());
    return val;
}

void CPrefab::Deserialize(JSON_Object* obj)
{
    const char* path = json_object_get_string(obj, "prefabSourcePath");
    prefabSourcePath = path ? path : "";
}
