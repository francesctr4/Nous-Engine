#include "Engine/Systems/ECS/Component/Types/CAudioListener/include/CAudioListener.h"

#include "Engine/Modules/ModuleAudio/include/ModuleAudio.h"
#include "Engine/Modules/ModuleScene/include/ModuleScene.h"
#include "Engine/Systems/ECS/Scene/include/Scene.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/ECS/Component/Types/CTransform/include/CTransform.h"
#include "Engine/Utils/Serialization/JsonFile/JsonObject.h"

ModuleScene* CAudioListener::GetModuleScene() const
{
    auto go = GetGameObject();
    Scene* scene = go.IsValid() ? go.GetScene() : nullptr;
    return scene ? scene->GetModuleScene() : nullptr;
}

ModuleAudio* CAudioListener::GetAudioModule() const
{
    ModuleScene* moduleScene = GetModuleScene();
    return moduleScene ? moduleScene->GetAudio() : nullptr;
}

void CAudioListener::OnUpdate(float /*deltaTime*/)
{
    if (!isMainListener)
        return;

    ModuleScene* moduleScene = GetModuleScene();
    ModuleAudio* audio = moduleScene ? moduleScene->GetAudio() : nullptr;
    if (!moduleScene || !audio)
        return;  // headless / test scene — no audio broker wired

    // Only drive the listener while the scene is simulating; voices only exist then.
    if (moduleScene->IsStopped())
        return;

    auto go = GetGameObject();
    auto* t = go.IsValid() ? go.TryGetComponent<CTransform>() : nullptr;
    if (!t)
        return;

    // Match CCamera's convention exactly: local position + orientation-derived basis.
    const glm::vec3 pos     = t->position;
    const glm::vec3 forward = t->GetForward();
    const glm::vec3 up      = t->GetUp();

    audio->SetListenerPosition (pos.x, pos.y, pos.z);
    audio->SetListenerDirection(forward.x, forward.y, forward.z);
    audio->SetListenerWorldUp  (up.x, up.y, up.z);
}

JsonObject CAudioListener::Serialize() const
{
    JsonObject root;
    root.Set("type",           GetType());
    root.Set("isMainListener", isMainListener);
    return root;
}

void CAudioListener::Deserialize(const JsonObject& obj)
{
    isMainListener = obj.GetBool("isMainListener", isMainListener);
}
