#pragma once

// Plain C++ helper class — no engine dependencies.
// SpinnerScript.cpp includes this header and holds one instance.
struct SpinnerComponent
{
    float speedDegPerSec = 90.0f;

    // Which axes to spin on (0 or 1 per axis).
    float axisX = 0.0f;
    float axisY = 1.0f;
    float axisZ = 0.0f;

    // Advance the accumulated angle by dt.
    void Tick(float dt);

    // Fill outX/Y/Z with the current Euler rotation produced by this spinner.
    void GetCurrentRotation(float& outX, float& outY, float& outZ) const;

private:
    float m_accumulatedDeg = 0.0f;
};
