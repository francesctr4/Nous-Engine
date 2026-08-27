#include <ECS/Component/Types/CAudioListener.h>

#include <ECS/ComponentServices.h>
#include <ECS/Scene/iSceneHost.h>
#include <AudioSystem/iAudioBroker.h>
#include <ECS/Scene/Scene.h>
#include <ECS/GameObject.h>
#include <ECS/Component/Types/CTransform.h>
#include "Engine/Utils/Serialization/JsonFile/JsonObject.h"

void CAudioListener::OnUpdate(float /*deltaTime*/)
{
    if (!isMainListener)
        return;

    const ComponentServices& s = Services();
    if (!s.host || !s.audio)
        return;  // headless / test scene — no audio broker wired

    // Only drive the listener while the scene is simulating; voices only exist then.
    // Guarded on IsStopped rather than !IsPlaying: a PAUSED scene keeps its voices
    // and their cursors, so the listener must stay current or a resumed scene pans
    // from a stale position.
    if (s.host->IsStopped())
        return;

    auto go = GetGameObject();
    auto* t = go.IsValid() ? go.TryGetComponent<CTransform>() : nullptr;
    if (!t)
        return;

    // Match CCamera's convention exactly: local position + orientation-derived basis.
    const glm::vec3 pos     = t->position;
    const glm::vec3 forward = t->GetForward();
    const glm::vec3 up      = t->GetUp();

    s.audio->SetListenerPosition (pos.x, pos.y, pos.z);
    s.audio->SetListenerDirection(forward.x, forward.y, forward.z);
    s.audio->SetListenerWorldUp  (up.x, up.y, up.z);
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
