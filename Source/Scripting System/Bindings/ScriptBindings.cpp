// File: LoggerBindings.cpp
#include "Scripting System/Bindings/ScriptBindings.h"
#include "Utils/Logger.h"
#include "Core/Application.h"
#include "Modules/ModuleInput.h"
#include "Modules/ModuleScene.h"
#include "ECS/Scene.h"

void ScriptBindings::SetupAllBindings(EngineAPI& api)
{
    SetupLoggerBindings(api.Logger);
    SetupInputBindings(api.Input);
    SetupGameObjectBindings(api.GameObject);
}

void ScriptBindings::SetupLoggerBindings(LoggerAPI& logger)
{
    logger.Trace = [](const char* msg, ...) {
        char buffer[1024];
        va_list args;
                va_start(args, msg);
        vsnprintf(buffer, sizeof(buffer), msg, args);
                va_end(args);
        NOUS_TRACE("[SCRIPT] %s", buffer);
    };

    logger.Debug = [](const char* msg, ...) {
        char buffer[1024];
        va_list args;
                va_start(args, msg);
        vsnprintf(buffer, sizeof(buffer), msg, args);
                va_end(args);
        NOUS_DEBUG("[SCRIPT] %s", buffer);
    };

    logger.Info = [](const char* msg, ...) {
        char buffer[1024];
        va_list args;
                va_start(args, msg);
        vsnprintf(buffer, sizeof(buffer), msg, args);
                va_end(args);
        NOUS_INFO("[SCRIPT] %s", buffer);
    };

    logger.Warn = [](const char* msg, ...) {
        char buffer[1024];
        va_list args;
                va_start(args, msg);
        vsnprintf(buffer, sizeof(buffer), msg, args);
                va_end(args);
        NOUS_WARN("[SCRIPT] %s", buffer);
    };

    logger.Error = [](const char* msg, ...) {
        char buffer[1024];
        va_list args;
                va_start(args, msg);
        vsnprintf(buffer, sizeof(buffer), msg, args);
                va_end(args);
        NOUS_ERROR("[SCRIPT] %s", buffer);
    };

    logger.Fatal = [](const char* msg, ...) {
        char buffer[1024];
        va_list args;
                va_start(args, msg);
        vsnprintf(buffer, sizeof(buffer), msg, args);
                va_end(args);
        NOUS_FATAL("[SCRIPT] %s", buffer);
    };
}

void ScriptBindings::SetupInputBindings(InputAPI &input)
{
    // Key state checking
    input.GetKey = [](int scancode) -> int {
        if (!External || !External->input) {
            return 0; // Return IDLE if input system not available
        }
        // Cast the engine's KeyState to int for scripting
        return static_cast<int>(External->input->GetKey(scancode));
    };

    // Mouse button checking
    input.GetMouseButton = [](int button) -> int {
        if (!External || !External->input) {
            return 0;
        }
        return static_cast<int>(External->input->GetMouseButton(button));
    };

    // Mouse position
    input.GetMousePosition = [](int* x, int* y) {
        if (!External || !External->input) {
            if (x) *x = 0;
            if (y) *y = 0;
            return;
        }
        if (x) *x = External->input->GetMouseX();
        if (y) *y = External->input->GetMouseY();
    };

    // Mouse motion
    input.GetMouseMotion = [](int* x, int* y) {
        if (!External || !External->input) {
            if (x) *x = 0;
            if (y) *y = 0;
            return;
        }
        if (x) *x = External->input->GetMouseXMotion();
        if (y) *y = External->input->GetMouseYMotion();
    };
}

void ScriptBindings::SetupGameObjectBindings(GameObjectAPI &gameObject)
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
