#ifndef NOUS_ENGINE_CPREFAB_H
#define NOUS_ENGINE_CPREFAB_H

#include "Engine/Systems/ECS/Component/Component.h"
#include "Engine/EngineExport.h"

#include <string>

// CPrefab marks a GameObject as the root of an instantiated prefab.
// It stores the path of the source .nprefab file so the scene can
// auto-reload it on load (RefreshPrefabInstances).
class CPrefab : public Component
{
public:
    COMPONENT_TYPE(CPrefab)

    std::string prefabSourcePath; // e.g. "Assets/Prefabs/MyPrefab.nprefab"

    NOUS_ENGINE_API JsonObject Serialize() const override;
    NOUS_ENGINE_API void Deserialize(const JsonObject& obj) override;
};

#endif // NOUS_ENGINE_CPREFAB_H
