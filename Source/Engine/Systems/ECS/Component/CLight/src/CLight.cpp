#include "Engine/Systems/ECS/Component/CLight/include/CLight.h"

#include <parson.h>
#include <string_view>

void CLight::OnUpdate(float /*deltaTime*/) {}

JSON_Value* CLight::Serialize() const
{
    JSON_Value*  objVal = json_value_init_object();
    JSON_Object* obj    = json_value_get_object(objVal);

    json_object_set_string(obj, "type", GetType().c_str());

    const char* lightTypeStr = (type == LightType::Spot)  ? "Spot"
                             : (type == LightType::Point) ? "Point"
                             :                              "Directional";
    json_object_set_string(obj, "lightType", lightTypeStr);
    json_object_set_number(obj, "colorR",    color.r);
    json_object_set_number(obj, "colorG",    color.g);
    json_object_set_number(obj, "colorB",    color.b);
    json_object_set_number(obj, "intensity", intensity);
    json_object_set_number(obj, "range",     range);

    if (type == LightType::Spot)
    {
        json_object_set_number(obj, "innerAngle", innerAngle);
        json_object_set_number(obj, "outerAngle", outerAngle);
    }

    return objVal;
}

void CLight::Deserialize(JSON_Object* obj)
{
    if (json_object_has_value(obj, "lightType"))
    {
        const char* lt = json_object_get_string(obj, "lightType");
        if (lt && std::string_view(lt) == "Spot")        type = LightType::Spot;
        else if (lt && std::string_view(lt) == "Point")  type = LightType::Point;
        else                                             type = LightType::Directional;
    }
    if (json_object_has_value(obj, "colorR"))     color.r    = static_cast<float>(json_object_get_number(obj, "colorR"));
    if (json_object_has_value(obj, "colorG"))     color.g    = static_cast<float>(json_object_get_number(obj, "colorG"));
    if (json_object_has_value(obj, "colorB"))     color.b    = static_cast<float>(json_object_get_number(obj, "colorB"));
    if (json_object_has_value(obj, "intensity"))  intensity  = static_cast<float>(json_object_get_number(obj, "intensity"));
    if (json_object_has_value(obj, "range"))      range      = static_cast<float>(json_object_get_number(obj, "range"));
    if (json_object_has_value(obj, "innerAngle")) innerAngle = static_cast<float>(json_object_get_number(obj, "innerAngle"));
    if (json_object_has_value(obj, "outerAngle")) outerAngle = static_cast<float>(json_object_get_number(obj, "outerAngle"));
}
