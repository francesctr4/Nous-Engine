#pragma once

#include "Globals.h"

#include "MathUtils.h"
#include "MathGeoLib/include/Geometry/Frustum.h"

class Camera 
{
public:

	Camera();
	~Camera();

	void SetPos(float3 xyz);
	void SetPos(float x, float y, float z);

	void UpdatePos(const float3& newPos);

	float3 GetPos() const;

	void SetNearPlane(float distance);
	float GetNearPlane();

	void SetFarPlane(float distance);
	float GetFarPlane();

	void SetFront(float3 front);
	void SetUp(float3 up);

	float3 GetFront() const;
	float3 GetUp() const;
	float3 GetRight() const;

	float GetHorizontalFOV() const;
	float GetVerticalFOV() const;

	void SetHorizontalFOV(float hfov);
	void SetVerticalFOV(float vfov);
	void SetBothFOV(float fov);

	float GetAspectRatio() const;
	void SetAspectRatio(float aspectRatio);

	float4x4 GetProjectionMatrix() const;
	float4x4 GetViewMatrix() const;

private:

	Frustum frustum;
};