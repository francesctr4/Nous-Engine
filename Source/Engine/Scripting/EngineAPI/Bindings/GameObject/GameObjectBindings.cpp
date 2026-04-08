#include "Engine/Scripting/EngineAPI/Bindings/GameObject/GameObjectBindings.h"

#include "Engine/Modules/ModuleScene/include/ModuleScene.h"
#include "Engine/Systems/ECS/Scene/include/Scene.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/ECS/Component/CTransform/include/CTransform.h"
#include "Engine/Core/Logger/Logger.h"

static ModuleScene* s_scene = nullptr;

void SetupGameObjectBindings(GameObjectAPI& gameObject, ModuleScene* moduleScene)
{
    s_scene = moduleScene;

    // Create binding - converts const char* to GameObjectID
    gameObject.Create = [](const char* name) -> uint32_t {
        if (!s_scene->activeScene) {
            NOUS_ERROR("No active scene to create GameObject in!");
            return 0;
        }
        uint32_t newID = s_scene->activeScene->CreateGameObjectID(name ? name : "GameObject");
        NOUS_DEBUG("Created GameObject '%s' with ID: %u", name, newID);
        return newID;
    };

    // Destroy binding - uses GameObjectID
    gameObject.Destroy = [](uint32_t id) {
        if (!s_scene->activeScene) {
            NOUS_ERROR("No active scene to destroy GameObject from!");
            return;
        }
        if (id == 0) {
            NOUS_WARN("Attempted to destroy GameObject with invalid ID 0");
            return;
        }
        NOUS_DEBUG("Destroying GameObject with ID: %u", id);
        s_scene->activeScene->DestroyGameObjectByID(id);
    };

    gameObject.SetPosition = [](uint32_t id, float x, float y, float z) {
        if (!s_scene || !s_scene->activeScene) {
            NOUS_ERROR("Scene not available for SetPosition!");
            return;
        }
        GameObject* go = s_scene->activeScene->GetGameObjectByID(id);
        if (!go) { NOUS_WARN("GameObject with ID %u not found for SetPosition!", id); return; }
        if (!go->HasComponent<CTransform>()) { NOUS_WARN("GameObject %u has no Transform component!", id); return; }
        auto& transform = go->GetComponent<CTransform>();
        transform.position = glm::vec3(x, y, z);
        transform.UpdateMatrix();
    };

    gameObject.SetRotation = [](uint32_t id, float x, float y, float z) {
        if (!s_scene || !s_scene->activeScene) {
            NOUS_ERROR("Scene not available for SetRotation!");
            return;
        }
        GameObject* go = s_scene->activeScene->GetGameObjectByID(id);
        if (!go) { NOUS_WARN("GameObject with ID %u not found for SetRotation!", id); return; }
        if (!go->HasComponent<CTransform>()) { NOUS_WARN("GameObject %u has no Transform component!", id); return; }
        auto& transform = go->GetComponent<CTransform>();
        transform.SetEulerRotation(glm::vec3(x, y, z));
        transform.UpdateMatrix();
    };

    gameObject.SetScale = [](uint32_t id, float x, float y, float z) {
        if (!s_scene || !s_scene->activeScene) {
            NOUS_ERROR("Scene not available for SetScale!");
            return;
        }
        GameObject* go = s_scene->activeScene->GetGameObjectByID(id);
        if (!go) { NOUS_WARN("GameObject ID %u not found for SetScale!", id); return; }
        if (!go->HasComponent<CTransform>()) { NOUS_WARN("GameObject %u has no Transform for SetScale!", id); return; }
        auto& transform = go->GetComponent<CTransform>();
        transform.scale = glm::vec3(x, y, z);
        transform.UpdateMatrix();
    };

    gameObject.GetPosition = [](uint32_t id, float* x, float* y, float* z) {
        if (!s_scene || !s_scene->activeScene) return;
        GameObject* go = s_scene->activeScene->GetGameObjectByID(id);
        if (!go || !go->HasComponent<CTransform>()) return;
        const auto& pos = go->GetComponent<CTransform>().position;
        if (x) *x = pos.x;
        if (y) *y = pos.y;
        if (z) *z = pos.z;
    };

    gameObject.GetRotation = [](uint32_t id, float* x, float* y, float* z) {
        if (!s_scene || !s_scene->activeScene) return;
        GameObject* go = s_scene->activeScene->GetGameObjectByID(id);
        if (!go || !go->HasComponent<CTransform>()) return;
        const glm::vec3 euler = go->GetComponent<CTransform>().GetEulerAngles();
        if (x) *x = euler.x;
        if (y) *y = euler.y;
        if (z) *z = euler.z;
    };

    gameObject.GetScale = [](uint32_t id, float* x, float* y, float* z) {
        if (!s_scene || !s_scene->activeScene) return;
        GameObject* go = s_scene->activeScene->GetGameObjectByID(id);
        if (!go || !go->HasComponent<CTransform>()) return;
        const auto& sc = go->GetComponent<CTransform>().scale;
        if (x) *x = sc.x;
        if (y) *y = sc.y;
        if (z) *z = sc.z;
    };

    gameObject.FindByName = [](const char* name) -> uint32_t {
        if (!s_scene || !s_scene->activeScene || !name) return 0;
        auto results = s_scene->activeScene->FindGameObjectsByName(name);
        return results.empty() ? 0 : results[0]->GetID();
    };
}
