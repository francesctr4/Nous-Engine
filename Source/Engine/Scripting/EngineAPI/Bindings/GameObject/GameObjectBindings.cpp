#include "Engine/Scripting/EngineAPI/Bindings/GameObject/GameObjectBindings.h"

#include "Engine/Core/Application.h"
#include "Engine/Modules/ModuleScene/include/ModuleScene.h"

#include "Engine/Systems/ECS/Scene/include/Scene.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/ECS/Component/CTransform/include/CTransform.h"
#include "Engine/Core/Logging System/Logger.h"

void SetupGameObjectBindings(GameObjectAPI &gameObject)
{
    // Create binding - converts const char* to GameObjectID
    gameObject.Create = [](const char* name) -> uint32_t {
        if (!External->scene->activeScene) {
            NOUS_ERROR("[SCRIPT] No active scene to create GameObject in!");
            return 0; // 0 is invalid ID
        }

        uint32_t newID = External->scene->activeScene->CreateGameObjectID(name ? name : "GameObject");
        NOUS_DEBUG("[SCRIPT] Created GameObject '%s' with ID: %u", name, newID);
        return newID;
    };

    // Destroy binding - uses GameObjectID
    gameObject.Destroy = [](uint32_t id) {
        if (!External->scene->activeScene) {
            NOUS_ERROR("[SCRIPT] No active scene to destroy GameObject from!");
            return;
        }

        if (id == 0) {
            NOUS_WARN("[SCRIPT] Attempted to destroy GameObject with invalid ID 0");
            return;
        }

        NOUS_DEBUG("[SCRIPT] Destroying GameObject with ID: %u", id);
        External->scene->activeScene->DestroyGameObjectByID(id);
    };

    // Bind the SetPosition function
    gameObject.SetPosition = [](uint32_t id, float x, float y, float z) {
        if (!External || !External->scene || !External->scene->activeScene) {
            NOUS_ERROR("[SCRIPT] Scene not available for SetPosition!");
            return;
        }

        // 1. Get the GameObject by ID
        GameObject* go = External->scene->activeScene->GetGameObjectByID(id);
        if (!go) {
            NOUS_WARN("[SCRIPT] GameObject with ID %u not found for SetPosition!", id);
            return;
        }

        // 2. Check for and get the Transform component
        if (!go->HasComponent<CTransform>()) {
            NOUS_WARN("[SCRIPT] GameObject %u has no Transform component!", id);
            return;
        }

        // 3. Set the new position
        auto& transform = go->GetComponent<CTransform>();
        transform.position = glm::vec3(x, y, z); // Assuming you use glm and your component has a 'position' member

        NOUS_DEBUG("[SCRIPT] Set position of GameObject %u to (%.2f, %.2f, %.2f)", id, x, y, z);
    };

    // Implement SetRotation, SetScale, GetPosition, etc. following the same pattern
    gameObject.SetRotation = [](uint32_t id, float x, float y, float z) {
        if (!External || !External->scene || !External->scene->activeScene) {
            NOUS_ERROR("[SCRIPT] Scene not available for SetPosition!");
            return;
        }

        // 1. Get the GameObject by ID
        GameObject* go = External->scene->activeScene->GetGameObjectByID(id);
        if (!go) {
            NOUS_WARN("[SCRIPT] GameObject with ID %u not found for SetPosition!", id);
            return;
        }

        // 2. Check for and get the Transform component
        if (!go->HasComponent<CTransform>()) {
            NOUS_WARN("[SCRIPT] GameObject %u has no Transform component!", id);
            return;
        }

        // 3. Set the new position
        auto& transform = go->GetComponent<CTransform>();
        transform.rotation = glm::vec3(x, y, z); // Assuming you use glm and your component has a 'position' member

        NOUS_DEBUG("[SCRIPT] Set position of GameObject %u to (%.2f, %.2f, %.2f)", id, x, y, z);
    };

    gameObject.SetScale = [](uint32_t id, float x, float y, float z) {
        if (!External || !External->scene || !External->scene->activeScene) {
            NOUS_ERROR("[SCRIPT] Scene not available for SetPosition!");
            return;
        }

        // 1. Get the GameObject by ID
        GameObject* go = External->scene->activeScene->GetGameObjectByID(id);
        if (!go) {
            NOUS_WARN("[SCRIPT] GameObject with ID %u not found for SetPosition!", id);
            return;
        }

        // 2. Check for and get the Transform component
        if (!go->HasComponent<CTransform>()) {
            NOUS_WARN("[SCRIPT] GameObject %u has no Transform component!", id);
            return;
        }

        // 3. Set the new position
        auto& transform = go->GetComponent<CTransform>();
        transform.scale = glm::vec3(x, y, z); // Assuming you use glm and your component has a 'position' member

        NOUS_DEBUG("[SCRIPT] Set position of GameObject %u to (%.2f, %.2f, %.2f)", id, x, y, z);
    };
}
