#include "Engine/Systems/ECS/Component/CPrefab/include/CPrefab.h"

#include "Engine/Utils/Serialization/JsonFile/JsonObject.h"

JsonObject CPrefab::Serialize() const
{
    JsonObject root;
    root.Set("type",             GetType());
    root.Set("prefabSourcePath", prefabSourcePath);
    return root;
}

void CPrefab::Deserialize(const JsonObject& obj)
{
    prefabSourcePath = obj.GetString("prefabSourcePath");
}
