#include <ECS/Component/Types/CPrefab/CPrefab.h>

#include <Utils/Serialization/JsonObject.h>

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
