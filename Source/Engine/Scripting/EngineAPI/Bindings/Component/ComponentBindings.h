#pragma once

#include <cstdint>

struct ComponentAPI
{
    bool (*HasLight)  (uint32_t goId) = nullptr;
    bool (*HasCamera) (uint32_t goId) = nullptr;
    bool (*HasMesh)   (uint32_t goId) = nullptr;
    bool (*HasScript) (uint32_t goId) = nullptr;
};

class IScriptSceneHost;
void SetupComponentBindings(ComponentAPI& component, IScriptSceneHost* sceneHost);
