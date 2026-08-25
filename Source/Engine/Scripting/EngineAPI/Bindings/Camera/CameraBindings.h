#pragma once

#include <cstdint>

struct CameraAPI
{
    // Returns the GO id of the first CCamera with isMainCamera=true; 0 if none.
    uint32_t (*GetMainCamera)() = nullptr;

    float (*GetFOV) (uint32_t goId) = nullptr;
    void  (*SetFOV) (uint32_t goId, float fov) = nullptr;

    float (*GetNear)(uint32_t goId) = nullptr;
    void  (*SetNear)(uint32_t goId, float nearPlane) = nullptr;

    float (*GetFar) (uint32_t goId) = nullptr;
    void  (*SetFar) (uint32_t goId, float farPlane) = nullptr;

    int  (*IsMain) (uint32_t goId) = nullptr;
    void (*SetMain)(uint32_t goId, int isMain) = nullptr;
};

class IScriptSceneHost;
void SetupCameraBindings(CameraAPI& camera, IScriptSceneHost* sceneHost);
