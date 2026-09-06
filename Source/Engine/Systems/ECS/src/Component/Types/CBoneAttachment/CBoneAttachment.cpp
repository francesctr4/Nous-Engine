#include <ECS/Component/Types/CBoneAttachment/CBoneAttachment.h>

#include <Utils/Serialization/JsonObject.h>

JsonObject CBoneAttachment::Serialize() const
{
    JsonObject root;
    root.Set("type",     GetType());
    root.Set("boneName", boneName);
    return root;
}

void CBoneAttachment::Deserialize(const JsonObject& obj)
{
    boneName = obj.GetString("boneName");

    // A new name means any warning already emitted names a bone this component no
    // longer refers to.
    warnedUnresolved = false;
}
