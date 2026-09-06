#pragma once

#include <ECS/Component/Component.h>
#include <EngineCore/EngineExport.h>

#include <cstdint>
#include <string>

// CPrefab marks a GameObject as the root of an instantiated prefab.
// It stores the path of the source .nprefab file so the scene can
// auto-reload it on load (RefreshPrefabInstances).
class CPrefab : public Component
{
public:
    COMPONENT_TYPE(CPrefab)

    std::string prefabSourcePath; // e.g. "Assets/Prefabs/MyPrefab.nprefab"

    // FNV-1a over the .nprefab bytes as of the last instantiate / update / apply.
    // 0 means "never synced" -- an instance from a scene saved before prefab
    // overrides existed, which the Update path migrates (see PrefabManager).
    //
    // SERIALIZED AS A HEX STRING, NOT A NUMBER. parson stores JSON numbers as
    // double; a uint64 above 2^53 does not round-trip, and the symptom is silent --
    // every instance reports stale forever.
    uint64_t syncedHash = 0;

    // Runtime only, never serialized: recomputed for every instance on scene load
    // by ModuleScene::UpdatePrefabStaleFlags. A persisted copy could only disagree
    // with the file on disk.
    bool isStale = false;

    NOUS_ENGINE_API JsonObject Serialize() const override;
    NOUS_ENGINE_API void Deserialize(const JsonObject& obj) override;
};
