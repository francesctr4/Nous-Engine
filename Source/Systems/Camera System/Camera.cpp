#include "Systems/Camera System/Camera.h"

#include <glm/gtc/matrix_transform.hpp>

Camera::Camera()
{
    position = glm::vec3(0.0f, 0.0f, 0.0f);
    front = glm::vec3(0.0f, 0.0f, -1.0f);
    up = glm::vec3(0.0f, 1.0f, 0.0f);
    nearPlane = 0.1f;
    farPlane = 1000.0f;
    verticalFov = 60.0f * NOUS_MathUtils::DEGTORAD;
    aspectRatio = static_cast<float>(WINDOW_WIDTH) / static_cast<float>(WINDOW_HEIGHT);
}

Camera::~Camera() {}

void Camera::SetPos(glm::vec3 xyz) { position = xyz; }
void Camera::SetPos(float x, float y, float z) { position = glm::vec3(x, y, z); }
void Camera::UpdatePos(const glm::vec3& newPos) { position += newPos; }
glm::vec3 Camera::GetPos() const { return position; }

void Camera::SetNearPlane(float distance) { nearPlane = distance; }
float Camera::GetNearPlane() { return nearPlane; }
void Camera::SetFarPlane(float distance) { farPlane = distance; }
float Camera::GetFarPlane() { return farPlane; }

void Camera::SetFront(glm::vec3 front) { this->front = glm::normalize(front); }
void Camera::SetUp(glm::vec3 up) { this->up = glm::normalize(up); }
glm::vec3 Camera::GetFront() const { return front; }
glm::vec3 Camera::GetUp() const { return up; }
glm::vec3 Camera::GetRight() const {
    return glm::normalize(glm::cross(front, up));
}

float Camera::GetVerticalFOV() const {
    return verticalFov * NOUS_MathUtils::RADTODEG;
}

float Camera::GetHorizontalFOV() const {
    float halfVertical = verticalFov * 0.5f;
    float halfHorizontal = atan(tan(halfVertical) * aspectRatio);
    return 2.0f * halfHorizontal * NOUS_MathUtils::RADTODEG;
}

void Camera::SetVerticalFOV(float vfov) {
    verticalFov = vfov * NOUS_MathUtils::DEGTORAD;
}

void Camera::SetHorizontalFOV(float hfov) {
    float halfHorizontalRad = (hfov * NOUS_MathUtils::DEGTORAD) * 0.5f;
    verticalFov = 2.0f * atan(tan(halfHorizontalRad) / aspectRatio);
}

void Camera::SetBothFOV(float fov) {
    verticalFov = fov * NOUS_MathUtils::DEGTORAD;
    aspectRatio = 1.0f;
}

float Camera::GetAspectRatio() const { return aspectRatio; }
void Camera::SetAspectRatio(float aspectRatio) {
    this->aspectRatio = aspectRatio;
}

glm::mat4x4 Camera::GetProjectionMatrix() const {
    return glm::perspective(verticalFov, aspectRatio, nearPlane, farPlane);
}

glm::mat4x4 Camera::GetViewMatrix() const {
    return glm::lookAt(position, position + front, up);
}
