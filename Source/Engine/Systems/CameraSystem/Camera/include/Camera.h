#ifndef CAMERA_H
#define CAMERA_H

#include <glm/glm.hpp>

#include "Engine/EngineExport.h"

class Camera
{
public:
    Camera();
    ~Camera();

    void SetPos(glm::vec3 xyz);
    void SetPos(float x, float y, float z);
    void UpdatePos(const glm::vec3& newPos);
    glm::vec3 GetPos() const;

    void SetNearPlane(float distance);
    NOUS_ENGINE_API float GetNearPlane();
    void SetFarPlane(float distance);
    NOUS_ENGINE_API float GetFarPlane();

    void SetFront(glm::vec3 front);
    void SetUp(glm::vec3 up);
    glm::vec3 GetFront() const;
    glm::vec3 GetUp() const;
    glm::vec3 GetRight() const;

    float GetHorizontalFOV() const;
    NOUS_ENGINE_API float GetVerticalFOV() const;
    void SetHorizontalFOV(float hfov);
    void SetVerticalFOV(float vfov);
    void SetBothFOV(float fov);

    float GetAspectRatio() const;
    void SetAspectRatio(float aspectRatio);

    NOUS_ENGINE_API glm::mat4 GetProjectionMatrix() const;
    NOUS_ENGINE_API glm::mat4 GetViewMatrix() const;

private:
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    float nearPlane;
    float farPlane;
    float verticalFov;   // In radians
    float aspectRatio;
};

#endif // CAMERA_H