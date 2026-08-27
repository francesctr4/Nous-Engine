#include "Engine/Scripting/EngineAPI/Bindings/Light/LightBindings.h"

#include "Engine/Scripting/iScriptSceneHost.h"
#include "Engine/Systems/ECS/Scene/include/Scene.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/ECS/Component/Types/CLight/include/CLight.h"
#include <Logger/Logger.h>

static IScriptSceneHost* s_scene = nullptr;

static CLight* GetLight(uint32_t id)
{
    if (!s_scene || !s_scene->GetActiveScene()) return nullptr;
    GameObject go = s_scene->GetActiveScene()->GetGameObjectByID(id);
    if (!go.IsValid()) { NOUS_WARN("[LightAPI] GameObject %u not found", id); return nullptr; }
    if (!go.HasComponent<CLight>()) { NOUS_WARN("[LightAPI] GameObject %u has no CLight", id); return nullptr; }
    return &go.GetComponent<CLight>();
}

void SetupLightBindings(LightAPI& light, IScriptSceneHost* sceneHost)
{
    s_scene = sceneHost;

    light.GetType = [](uint32_t id) -> int {
        const CLight* l = GetLight(id);
        return l ? static_cast<int>(l->type) : -1;
    };

    light.SetType = [](uint32_t id, int type) {
        CLight* l = GetLight(id);
        if (l) l->type = static_cast<LightType>(type);
    };

    light.GetColor = [](uint32_t id, float* r, float* g, float* b) {
        const CLight* l = GetLight(id);
        if (!l) return;
        if (r) *r = l->color.r;
        if (g) *g = l->color.g;
        if (b) *b = l->color.b;
    };

    light.SetColor = [](uint32_t id, float r, float g, float b) {
        CLight* l = GetLight(id);
        if (l) l->color = glm::vec3(r, g, b);
    };

    light.GetIntensity = [](uint32_t id) -> float {
        const CLight* l = GetLight(id);
        return l ? l->intensity : 0.0f;
    };

    light.SetIntensity = [](uint32_t id, float intensity) {
        CLight* l = GetLight(id);
        if (l) l->intensity = intensity;
    };

    light.GetRange = [](uint32_t id) -> float {
        const CLight* l = GetLight(id);
        return l ? l->range : 0.0f;
    };

    light.SetRange = [](uint32_t id, float range) {
        CLight* l = GetLight(id);
        if (l) l->range = range;
    };
}
