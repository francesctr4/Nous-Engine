#ifndef NOUS_ENGINE_GAMEOBJECTBINDINGS_H
#define NOUS_ENGINE_GAMEOBJECTBINDINGS_H

#include <cstdint>

struct GameObjectAPI
{
    uint32_t (*Create)(const char* name) = nullptr;
    void (*Destroy)(uint32_t UID) = nullptr;

    void (*SetPosition)(uint32_t id, float x, float y, float z) = nullptr;
    void (*SetRotation)(uint32_t id, float x, float y, float z) = nullptr;
    void (*SetScale)(uint32_t id, float x, float y, float z) = nullptr;
};

// Setup function for this specific API
void SetupGameObjectBindings(GameObjectAPI& gameObject);

#endif //NOUS_ENGINE_GAMEOBJECTBINDINGS_H
