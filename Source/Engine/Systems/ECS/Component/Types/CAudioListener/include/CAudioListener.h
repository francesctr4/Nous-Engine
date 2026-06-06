#pragma once

#include "Engine/Systems/ECS/Component/Component.h"
#include "Engine/EngineExport.h"

class ModuleScene;
class ModuleAudio;

/**
 * @brief Audio listener ("the ears") component.
 *
 * When isMainListener is true and the scene is simulating, OnUpdate pushes the
 * sibling CTransform's world position / forward / up to the audio engine each
 * frame, so spatialized CAudioSources pan and attenuate relative to it. Decoupled
 * from CCamera so the listener can live on a different object than the camera.
 *
 * Only one CAudioListener should have isMainListener set; ModuleAudio warns once
 * if more than one is active in a frame (last writer wins).
 */
class CAudioListener : public Component {
public:
    COMPONENT_TYPE(CAudioListener)

    bool isMainListener = true;

    NOUS_ENGINE_API void OnUpdate(float deltaTime) override;

    NOUS_ENGINE_API JsonObject Serialize() const override;
    NOUS_ENGINE_API void       Deserialize(const JsonObject& obj) override;

private:
    ModuleScene* GetModuleScene() const;
    ModuleAudio* GetAudioModule() const;
};
