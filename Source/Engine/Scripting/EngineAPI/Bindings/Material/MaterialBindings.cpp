#include "Engine/Scripting/EngineAPI/Bindings/Material/MaterialBindings.h"

#include "Engine/Modules/ModuleScene/include/ModuleScene.h"
#include "Engine/Systems/ECS/Scene/include/Scene.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/ECS/Component/CMaterial/include/CMaterial.h"
#include "Engine/Systems/ResourceManager/Types/ResourceMaterial/include/ResourceMaterial.h"
#include "Engine/Core/Logger/Logger.h"

static ModuleScene* s_scene = nullptr;

static ResourceMaterial* GetMaterial(uint32_t id)
{
    if (!s_scene || !s_scene->activeScene) return nullptr;
    GameObject go = s_scene->activeScene->GetGameObjectByID(id);
    if (!go.IsValid()) { NOUS_WARN("[MaterialAPI] GameObject %u not found", id); return nullptr; }
    if (!go.HasComponent<CMaterial>()) { NOUS_WARN("[MaterialAPI] GameObject %u has no CMaterial", id); return nullptr; }
    ResourceMaterial* mat = go.GetComponent<CMaterial>().material;
    if (!mat) { NOUS_WARN("[MaterialAPI] CMaterial on GameObject %u has no ResourceMaterial", id); return nullptr; }
    return mat;
}

void SetupMaterialBindings(MaterialAPI& material, ModuleScene* moduleScene)
{
    s_scene = moduleScene;

    material.GetFloat = [](uint32_t id, const char* name) -> float {
        const ResourceMaterial* mat = GetMaterial(id);
        if (!mat || !name) return 0.0f;
        auto it = mat->uniformValues.find(name);
        if (it == mat->uniformValues.end()) return 0.0f;
        return it->second.fdata.x;
    };

    material.SetFloat = [](uint32_t id, const char* name, float value) {
        ResourceMaterial* mat = GetMaterial(id);
        if (!mat || !name) return;
        auto& uv = mat->uniformValues[name];
        uv.type     = UniformValueType::Float;
        uv.fdata.x  = value;
    };

    material.GetVec3 = [](uint32_t id, const char* name, float* x, float* y, float* z) {
        const ResourceMaterial* mat = GetMaterial(id);
        if (!mat || !name) return;
        auto it = mat->uniformValues.find(name);
        if (it == mat->uniformValues.end()) return;
        const glm::vec4& v = it->second.fdata;
        if (x) *x = v.x;
        if (y) *y = v.y;
        if (z) *z = v.z;
    };

    material.SetVec3 = [](uint32_t id, const char* name, float x, float y, float z) {
        ResourceMaterial* mat = GetMaterial(id);
        if (!mat || !name) return;
        auto& uv = mat->uniformValues[name];
        uv.type  = UniformValueType::Vec3;
        uv.fdata = glm::vec4(x, y, z, uv.fdata.w);
    };

    material.GetVec4 = [](uint32_t id, const char* name, float* x, float* y, float* z, float* w) {
        const ResourceMaterial* mat = GetMaterial(id);
        if (!mat || !name) return;
        auto it = mat->uniformValues.find(name);
        if (it == mat->uniformValues.end()) return;
        const glm::vec4& v = it->second.fdata;
        if (x) *x = v.x;
        if (y) *y = v.y;
        if (z) *z = v.z;
        if (w) *w = v.w;
    };

    material.SetVec4 = [](uint32_t id, const char* name, float x, float y, float z, float w) {
        ResourceMaterial* mat = GetMaterial(id);
        if (!mat || !name) return;
        auto& uv = mat->uniformValues[name];
        uv.type  = UniformValueType::Vec4;
        uv.fdata = glm::vec4(x, y, z, w);
    };
}
