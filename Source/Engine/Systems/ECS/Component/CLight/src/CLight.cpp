#include "Engine/Systems/ECS/Component/CLight/include/CLight.h"

#include "Engine/Utils/Serialization/JsonFile/JsonObject.h"
#include <string_view>

void CLight::OnUpdate(float /*deltaTime*/) {}

JsonObject CLight::Serialize() const
{
    JsonObject root;
    root.Set("type", GetType());

    const char* lightTypeStr = (type == LightType::Spot)  ? "Spot"
                             : (type == LightType::Point) ? "Point"
                             :                              "Directional";
    root.Set("lightType", lightTypeStr);
    root.Set("colorR",    color.r);
    root.Set("colorG",    color.g);
    root.Set("colorB",    color.b);
    root.Set("intensity", intensity);
    root.Set("range",     range);

    if (type == LightType::Spot)
    {
        root.Set("innerAngle", innerAngle);
        root.Set("outerAngle", outerAngle);
    }

    return root;
}

void CLight::Deserialize(const JsonObject& obj)
{
    if (obj.HasKey("lightType"))
    {
        const std::string lt = obj.GetString("lightType");
        if (lt == "Spot")        type = LightType::Spot;
        else if (lt == "Point")  type = LightType::Point;
        else                     type = LightType::Directional;
    }
    color.r    = obj.GetFloat("colorR",     color.r);
    color.g    = obj.GetFloat("colorG",     color.g);
    color.b    = obj.GetFloat("colorB",     color.b);
    intensity  = obj.GetFloat("intensity",  intensity);
    range      = obj.GetFloat("range",      range);
    innerAngle = obj.GetFloat("innerAngle", innerAngle);
    outerAngle = obj.GetFloat("outerAngle", outerAngle);
}
