#include "SpinnerComponent.h"

void SpinnerComponent::Tick(float dt)
{
    m_accumulatedDeg += speedDegPerSec * dt;
    if (m_accumulatedDeg >= 360.0f)
        m_accumulatedDeg -= 360.0f;
}

void SpinnerComponent::GetCurrentRotation(float& outX, float& outY, float& outZ) const
{
    outX = axisX * m_accumulatedDeg;
    outY = axisY * m_accumulatedDeg;
    outZ = axisZ * m_accumulatedDeg;
}

