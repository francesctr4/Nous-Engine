#include <ECS/Component/Types/CPrefabLink/CPrefabLink.h>

#include <Utils/Serialization/JsonObject.h>

JsonObject CPrefabLink::Serialize() const
{
    JsonObject root;
    root.Set("type",           GetType());
    root.Set("prefabObjectID", static_cast<double>(prefabObjectID));
    return root;
}

void CPrefabLink::Deserialize(const JsonObject& obj)
{
    // A uint32_t is exactly representable as a double, so this round-trips. The
    // 64-bit CPrefab::syncedHash is NOT, and deliberately does not copy this.
    prefabObjectID = static_cast<uint32_t>(obj.GetDouble("prefabObjectID", 0.0));
}
