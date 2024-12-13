#include "Camera.h"

Camera::Camera()
{
	frustum.type = FrustumType::PerspectiveFrustum;
	frustum.pos = float3(0,0,20);
	frustum.front = -float3::unitZ;
	frustum.up = float3::unitY;

    // Default values for near and far planes
    frustum.nearPlaneDistance = 0.1f;
    frustum.farPlaneDistance = 1000.0f;

    // Default vertical FOV and aspect ratio
    frustum.verticalFov = 60.0f * NOUS_MathUtils::DEGTORAD; // 60 degrees in radians
    frustum.horizontalFov = 2.0f * atanf(tanf(frustum.verticalFov * 0.5f) * ((float)WINDOW_WIDTH / (float)WINDOW_HEIGHT));
}

Camera::~Camera()
{

}

void Camera::SetPos(float3 xyz)
{
	frustum.pos = xyz;
}

void Camera::SetPos(float x, float y, float z)
{
	frustum.pos = float3(x, y, z);
}

void Camera::UpdatePos(const float3& newPos)
{
	frustum.pos += newPos;
}

float3 Camera::GetPos() const
{
	return frustum.pos;
}

void Camera::SetNearPlane(float distance)
{
	frustum.nearPlaneDistance = distance;
}

float Camera::GetNearPlane()
{
	return frustum.nearPlaneDistance;
}

void Camera::SetFarPlane(float distance)
{
	frustum.farPlaneDistance = distance;
}

float Camera::GetFarPlane()
{
	return frustum.farPlaneDistance;
}

void Camera::SetFront(float3 front)
{
	frustum.front = front;
}

void Camera::SetUp(float3 up)
{
	frustum.up = up;
}

float3 Camera::GetFront() const
{
	return frustum.front;
}

float3 Camera::GetUp() const
{
	return frustum.up;
}

float3 Camera::GetRight() const
{
	return frustum.WorldRight();
}

float Camera::GetHorizontalFOV() const
{
	return frustum.horizontalFov * NOUS_MathUtils::RADTODEG;
}

float Camera::GetVerticalFOV() const
{
	return frustum.verticalFov * NOUS_MathUtils::RADTODEG;
}

void Camera::SetHorizontalFOV(float hfov)
{
	frustum.horizontalFov = hfov * NOUS_MathUtils::DEGTORAD;
}

void Camera::SetVerticalFOV(float vfov)
{
	frustum.verticalFov = vfov * NOUS_MathUtils::DEGTORAD;
}

void Camera::SetBothFOV(float fov)
{
	frustum.horizontalFov = fov * NOUS_MathUtils::DEGTORAD;
	frustum.verticalFov = fov * NOUS_MathUtils::DEGTORAD;
}

float Camera::GetAspectRatio() const
{
	return frustum.AspectRatio();
}

void Camera::SetAspectRatio(float aspectRatio)
{
	frustum.horizontalFov = 2.f * atanf(tanf(frustum.verticalFov * 0.5f) * aspectRatio);
}

float4x4 Camera::GetProjectionMatrix() const
{
	return frustum.ProjectionMatrix().Transposed();
}

float4x4 Camera::GetViewMatrix() const
{
	return float4x4(frustum.ViewMatrix()).Transposed();
}
