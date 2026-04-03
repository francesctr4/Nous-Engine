#include "Engine/Systems/ECS/Component/CLight/include/CLight.h"

#include <parson.h>
#include <string>

void CLight::OnUpdate(float /*deltaTime*/) {}

JSON_Value* CLight::Serialize() const
{
    JSON_Value*  objVal = json_value_init_object();
    JSON_Object* obj    = json_value_get_object(objVal);

    json_object_set_string(obj, "type",      GetType().c_str());
    json_object_set_string(obj, "lightType", type == LightType::Directional ? "Directional" : "Point");
    json_object_set_number(obj, "colorR",    color.r);
    json_object_set_number(obj, "colorG",    color.g);
    json_object_set_number(obj, "colorB",    color.b);
    json_object_set_number(obj, "intensity", intensity);
    json_object_set_number(obj, "range",     range);

    return objVal;
}

void CLight::Deserialize(JSON_Object* obj)
{
    if (json_object_has_value(obj, "lightType"))
    {
        const char* lt = json_object_get_string(obj, "lightType");
        type = (lt && std::string(lt) == "Point") ? LightType::Point : LightType::Directional;
    }
    if (json_object_has_value(obj, "colorR"))    color.r   = static_cast<float>(json_object_get_number(obj, "colorR"));
    if (json_object_has_value(obj, "colorG"))    color.g   = static_cast<float>(json_object_get_number(obj, "colorG"));
    if (json_object_has_value(obj, "colorB"))    color.b   = static_cast<float>(json_object_get_number(obj, "colorB"));
    if (json_object_has_value(obj, "intensity")) intensity = static_cast<float>(json_object_get_number(obj, "intensity"));
    if (json_object_has_value(obj, "range"))     range     = static_cast<float>(json_object_get_number(obj, "range"));
}
