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

    // Transform — direction vectors (derived from orientation quaternion)
    void (*GetForward)(uint32_t id, float* x, float* y, float* z) = nullptr;
    void (*GetRight)  (uint32_t id, float* x, float* y, float* z) = nullptr;
    void (*GetUp)     (uint32_t id, float* x, float* y, float* z) = nullptr;

    // Transform — relative movement (world-space delta)
    void (*Translate)(uint32_t id, float dx, float dy, float dz) = nullptr;

    // Transform — rotation delta in Euler degrees (world-space pre-multiply)
    void (*Rotate)(uint32_t id, float dx, float dy, float dz) = nullptr;

    // Hierarchy
    uint32_t (*GetParent)    (uint32_t id)                 = nullptr; // 0 if no parent
    void     (*SetParent)    (uint32_t id, uint32_t parentId) = nullptr; // 0 = detach
    uint32_t (*GetChildCount)(uint32_t id)                 = nullptr;
    uint32_t (*GetChild)     (uint32_t id, uint32_t index) = nullptr; // 0 if out of range

    // Name
    void (*GetName)(uint32_t id, char* buffer, int bufferSize) = nullptr;
    void (*SetName)(uint32_t id, const char* name)             = nullptr;
};

class ModuleScene;

// Setup function for this specific API
void SetupGameObjectBindings(GameObjectAPI& gameObject, ModuleScene* moduleScene);

#endif //NOUS_ENGINE_GAMEOBJECTBINDINGS_H
