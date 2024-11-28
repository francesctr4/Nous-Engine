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
    frustum.verticalFov = 45.0f * NOUS_MathUtils::DEGTORAD; // 45 degrees in radians
    frustum.horizontalFov = 2.0f * atanf(tanf(frustum.verticalFov * 0.5f) * ((float)WINDOW_WIDTH / (float)WINDOW_HEIGHT));
}

Camera::~Camera()
{

}

void Camera::UpdatePos(float3 newPos)
{
	frustum.pos += newPos;
}

float3 Camera::GetPos() const
{
	return frustum.pos;
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
