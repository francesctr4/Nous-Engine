#include <Scripting/EngineAPI/Bindings/AudioBindings.h>

#include <Scripting/iScriptSceneHost.h>
#include <ECS/Scene/Scene.h>
#include <ECS/GameObject.h>
#include <ECS/Component/Types/CAudioSource/CAudioSource.h>
#include <Logger/Logger.h>

static IScriptSceneHost* s_scene = nullptr;

static CAudioSource* GetSource(uint32_t id)
{
    if (!s_scene || !s_scene->GetActiveScene()) return nullptr;
    GameObject go = s_scene->GetActiveScene()->GetGameObjectByID(id);
    if (!go.IsValid()) { NOUS_WARN("[AudioAPI] GameObject %u not found", id); return nullptr; }
    if (!go.HasComponent<CAudioSource>()) { NOUS_WARN("[AudioAPI] GameObject %u has no CAudioSource", id); return nullptr; }
    return &go.GetComponent<CAudioSource>();
}

void SetupAudioBindings(AudioAPI& audio, IScriptSceneHost* sceneHost)
{
    s_scene = sceneHost;

    audio.Play = [](uint32_t id) {
        if (CAudioSource* s = GetSource(id)) s->Play();
    };

    audio.Stop = [](uint32_t id) {
        if (CAudioSource* s = GetSource(id)) s->Stop();
    };

    audio.IsPlaying = [](uint32_t id) -> bool {
        const CAudioSource* s = GetSource(id);
        return s && s->IsVoicePlaying();
    };

    audio.SetVolume = [](uint32_t id, float volume) {
        if (CAudioSource* s = GetSource(id)) s->volume = volume;
    };

    audio.SetPitch = [](uint32_t id, float pitch) {
        if (CAudioSource* s = GetSource(id)) s->pitch = pitch;
    };
}
