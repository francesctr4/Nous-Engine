#ifndef NOUS_ENGINE_GAMEOBJECTBINDINGS_H
#define NOUS_ENGINE_GAMEOBJECTBINDINGS_H

#include <cstdint>

struct GameObjectAPI
{
    uint32_t (*Create)(const char* name) = nullptr;
    void     (*Destroy)(uint32_t id)     = nullptr;

    // Transform setters
    void (*SetPosition)(uint32_t id, float x, float y, float z) = nullptr;
    void (*SetRotation)(uint32_t id, float x, float y, float z) = nullptr;
    void (*SetScale)   (uint32_t id, float x, float y, float z) = nullptr;

    // Transform getters (output via pointer arguments)
    void (*GetPosition)(uint32_t id, float* x, float* y, float* z) = nullptr;
    void (*GetRotation)(uint32_t id, float* x, float* y, float* z) = nullptr;
    void (*GetScale)   (uint32_t id, float* x, float* y, float* z) = nullptr;

    // Lookup — returns 0 if not found
    uint32_t (*FindByName)(const char* name) = nullptr;
};

// Setup function for this specific API
void SetupGameObjectBindings(GameObjectAPI& gameObject);

#endif //NOUS_ENGINE_GAMEOBJECTBINDINGS_H
