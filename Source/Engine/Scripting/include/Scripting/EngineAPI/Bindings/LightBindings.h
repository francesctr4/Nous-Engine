#pragma once

#include <cstdint>

// Light type constants exposed to scripts — values match LightType enum in CLight.h.
typedef enum NousLightType : int
{
    NOUS_LIGHT_DIRECTIONAL = 0,
    NOUS_LIGHT_POINT       = 1,
    NOUS_LIGHT_SPOT        = 2,
} NousLightType;

struct LightAPI
{
    // Type
    int  (*GetType)(uint32_t id) = nullptr;
    void (*SetType)(uint32_t id, int type) = nullptr;

    // Color (RGB, each component 0–1)
    void (*GetColor)(uint32_t id, float* r, float* g, float* b) = nullptr;
    void (*SetColor)(uint32_t id, float r, float g, float b) = nullptr;

    // Intensity (multiplier applied on top of color)
    float (*GetIntensity)(uint32_t id) = nullptr;
    void  (*SetIntensity)(uint32_t id, float intensity) = nullptr;

    // Range — point and spot lights only; ignored for directional
    float (*GetRange)(uint32_t id) = nullptr;
    void  (*SetRange)(uint32_t id, float range) = nullptr;
};

class IScriptSceneHost;
void SetupLightBindings(LightAPI& light, IScriptSceneHost* sceneHost);
