#include <Scripting/EngineAPI/Bindings/AnimatorBindings.h>

#include <ECS/Component/Types/CAnimator/CAnimator.h>
#include <ECS/GameObject.h>
#include <ECS/Scene/Scene.h>
#include <Logger/Logger.h>
#include <Scripting/iScriptSceneHost.h>

#include <string_view>

static IScriptSceneHost* s_scene = nullptr;

static CAnimator* GetAnimator(uint32_t id)
{
    if (!s_scene || !s_scene->GetActiveScene()) return nullptr;
    GameObject go = s_scene->GetActiveScene()->GetGameObjectByID(id);
    if (!go.IsValid()) { NOUS_WARN("[AnimatorAPI] GameObject %u not found", id); return nullptr; }
    if (!go.HasComponent<CAnimator>()) { NOUS_WARN("[AnimatorAPI] GameObject %u has no CAnimator", id); return nullptr; }
    return &go.GetComponent<CAnimator>();
}

void SetupAnimatorBindings(AnimatorAPI& animator, IScriptSceneHost* sceneHost)
{
    s_scene = sceneHost;

    animator.SetFloat = [](uint32_t id, const char* name, float value) {
        if (CAnimator* a = GetAnimator(id); a && name) a->parameters.SetFloat(name, value);
    };

    animator.GetFloat = [](uint32_t id, const char* name) -> float {
        const CAnimator* a = GetAnimator(id);
        return (a && name) ? a->parameters.GetFloat(name) : 0.0f;
    };

    animator.SetBool = [](uint32_t id, const char* name, bool value) {
        if (CAnimator* a = GetAnimator(id); a && name) a->parameters.SetBool(name, value);
    };

    animator.GetBool = [](uint32_t id, const char* name) -> bool {
        const CAnimator* a = GetAnimator(id);
        return (a && name) ? a->parameters.GetBool(name) : false;
    };

    animator.SetTrigger = [](uint32_t id, const char* name) {
        if (CAnimator* a = GetAnimator(id); a && name) a->parameters.SetTrigger(name);
    };

    animator.ResetTrigger = [](uint32_t id, const char* name) {
        if (CAnimator* a = GetAnimator(id); a && name) a->parameters.ResetTrigger(name);
    };

    animator.CrossFade = [](uint32_t id, const char* name, float fadeSeconds) -> bool {
        CAnimator* a = GetAnimator(id);
        return (a && name) ? a->CrossFade(name, fadeSeconds) : false;
    };

    animator.IsFading = [](uint32_t id) -> bool {
        const CAnimator* a = GetAnimator(id);
        return a && a->IsFading();
    };

    animator.GetCurrentState = [](uint32_t id, char* buffer, int bufferSize) {
        if (!buffer || bufferSize <= 0) return;
        buffer[0] = '\0';   // "no state" and "no animator" read the same, by design

        const CAnimator* a = GetAnimator(id);
        if (!a) return;

        // A string_view over the state's own name, NOT null-terminated -- so it is
        // copied by length. strncpy on .data() would read past the view.
        const std::string_view name = a->GetCurrentStateName();
        if (name.empty()) return;

        const size_t copied = name.copy(buffer, static_cast<size_t>(bufferSize) - 1);
        buffer[copied] = '\0';
    };

    animator.GetNormalizedTime = [](uint32_t id) -> float {
        const CAnimator* a = GetAnimator(id);
        return a ? a->GetNormalizedTime() : 0.0f;
    };

    animator.SetSpeed = [](uint32_t id, float speed) {
        if (CAnimator* a = GetAnimator(id)) a->speedMultiplier = speed;
    };

    animator.GetSpeed = [](uint32_t id) -> float {
        const CAnimator* a = GetAnimator(id);
        return a ? a->speedMultiplier : 0.0f;
    };
}
