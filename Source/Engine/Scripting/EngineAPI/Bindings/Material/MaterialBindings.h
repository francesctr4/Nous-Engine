#pragma once

#include <cstdint>

struct MaterialAPI
{
    // Float uniform
    float (*GetFloat)(uint32_t id, const char* name) = nullptr;
    void  (*SetFloat)(uint32_t id, const char* name, float value) = nullptr;

    // Vec3 uniform (RGB color, direction, etc.)
    void (*GetVec3)(uint32_t id, const char* name, float* x, float* y, float* z) = nullptr;
    void (*SetVec3)(uint32_t id, const char* name, float x, float y, float z) = nullptr;

    // Vec4 uniform (RGBA color, etc.)
    void (*GetVec4)(uint32_t id, const char* name, float* x, float* y, float* z, float* w) = nullptr;
    void (*SetVec4)(uint32_t id, const char* name, float x, float y, float z, float w) = nullptr;
};

class ModuleScene;
void SetupMaterialBindings(MaterialAPI& material, ModuleScene* moduleScene);
