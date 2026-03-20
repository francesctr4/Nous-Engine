#include "Engine/Scripting/EngineAPI/Bindings/GameObject/GameObjectBindings.h"

#include "Engine/Core/Application.h"
#include "Engine/Modules/ModuleScene/include/ModuleScene.h"

#include "Engine/Systems/ECS/Scene/include/Scene.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/ECS/Component/CTransform/include/CTransform.h"
#include "Engine/Core/Logger/Logger.h"

void SetupGameObjectBindings(GameObjectAPI &gameObject)
{
    // Create binding - converts const char* to GameObjectID
    gameObject.Create = [](const char* name) -> uint32_t {
        if (!External->scene->activeScene) {
            NOUS_ERROR("No active scene to create GameObject in!");
            return 0; // 0 is invalid ID
        }

        uint32_t newID = External->scene->activeScene->CreateGameObjectID(name ? name : "GameObject");
        NOUS_DEBUG("Created GameObject '%s' with ID: %u", name, newID);
        return newID;
    };

    // Destroy binding - uses GameObjectID
    gameObject.Destroy = [](uint32_t id) {
        if (!External->scene->activeScene) {
            NOUS_ERROR("No active scene to destroy GameObject from!");
            return;
        }

        if (id == 0) {
            NOUS_WARN("Attempted to destroy GameObject with invalid ID 0");
            return;
        }

        NOUS_DEBUG("Destroying GameObject with ID: %u", id);
        External->scene->activeScene->DestroyGameObjectByID(id);
    };

    // Bind the SetPosition function
    gameObject.SetPosition = [](uint32_t id, float x, float y, float z) {
        if (!External || !External->scene || !External->scene->activeScene) {
            NOUS_ERROR("Scene not available for SetPosition!");
            return;
        }

        // 1. Get the GameObject by ID
        GameObject* go = External->scene->activeScene->GetGameObjectByID(id);
        if (!go) {
            NOUS_WARN("GameObject with ID %u not found for SetPosition!", id);
            return;
        }

        // 2. Check for and get the Transform component
        if (!go->HasComponent<CTransform>()) {
            NOUS_WARN("GameObject %u has no Transform component!", id);
            return;
        }

        // 3. Set the new position
        auto& transform = go->GetComponent<CTransform>();
        transform.position = glm::vec3(x, y, z);
        transform.UpdateMatrix();

        NOUS_DEBUG("Set position of GameObject %u to (%.2f, %.2f, %.2f)", id, x, y, z);
    };

    // Implement SetRotation, SetScale, GetPosition, etc. following the same pattern
    gameObject.SetRotation = [](uint32_t id, float x, float y, float z) {
        if (!External || !External->scene || !External->scene->activeScene) {
            NOUS_ERROR("Scene not available for SetPosition!");
            return;
        }

        // 1. Get the GameObject by ID
        GameObject* go = External->scene->activeScene->GetGameObjectByID(id);
        if (!go) {
            NOUS_WARN("GameObject with ID %u not found for SetPosition!", id);
            return;
        }

        // 2. Check for and get the Transform component
        if (!go->HasComponent<CTransform>()) {
            NOUS_WARN("GameObject %u has no Transform component!", id);
            return;
        }

        // 3. Set the new position
        auto& transform = go->GetComponent<CTransform>();
        transform.SetEulerRotation(glm::vec3(x, y, z));
        transform.UpdateMatrix();

        NOUS_TRACE("Set rotation of GameObject %u to (%.2f, %.2f, %.2f)", id, x, y, z);
    };

    gameObject.SetScale = [](uint32_t id, float x, float y, float z) {
        if (!External || !External->scene || !External->scene->activeScene) {
            NOUS_ERROR("Scene not available for SetScale!");
            return;
        }
        GameObject* go = External->scene->activeScene->GetGameObjectByID(id);
        if (!go) { NOUS_WARN("GameObject ID %u not found for SetScale!", id); return; }
        if (!go->HasComponent<CTransform>()) {
            NOUS_WARN("GameObject %u has no Transform for SetScale!", id); return;
        }
        auto& transform = go->GetComponent<CTransform>();
        transform.scale = glm::vec3(x, y, z);
        transform.UpdateMatrix();
        NOUS_TRACE("Set scale of GameObject %u to (%.2f, %.2f, %.2f)", id, x, y, z);
    };

    gameObject.GetPosition = [](uint32_t id, float* x, float* y, float* z) {
        if (!External || !External->scene || !External->scene->activeScene) return;
        GameObject* go = External->scene->activeScene->GetGameObjectByID(id);
        if (!go || !go->HasComponent<CTransform>()) return;
        const auto& pos = go->GetComponent<CTransform>().position;
        if (x) *x = pos.x;
        if (y) *y = pos.y;
        if (z) *z = pos.z;
    };

    gameObject.GetRotation = [](uint32_t id, float* x, float* y, float* z) {
        if (!External || !External->scene || !External->scene->activeScene) return;
        GameObject* go = External->scene->activeScene->GetGameObjectByID(id);
        if (!go || !go->HasComponent<CTransform>()) return;
        const glm::vec3 euler = go->GetComponent<CTransform>().GetEulerAngles();
        if (x) *x = euler.x;
        if (y) *y = euler.y;
        if (z) *z = euler.z;
    };

    gameObject.GetScale = [](uint32_t id, float* x, float* y, float* z) {
        if (!External || !External->scene || !External->scene->activeScene) return;
        GameObject* go = External->scene->activeScene->GetGameObjectByID(id);
        if (!go || !go->HasComponent<CTransform>()) return;
        const auto& sc = go->GetComponent<CTransform>().scale;
        if (x) *x = sc.x;
        if (y) *y = sc.y;
        if (z) *z = sc.z;
    };

    gameObject.FindByName = [](const char* name) -> uint32_t {
        if (!External || !External->scene || !External->scene->activeScene || !name) return 0;
        auto results = External->scene->activeScene->FindGameObjectsByName(name);
        return results.empty() ? 0 : results[0]->GetID();
    };
}
