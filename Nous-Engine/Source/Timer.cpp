#include "Timer.h"

Timer::Timer()
{
    currentState = TimerState::STOPPED;
    started_at = std::chrono::steady_clock::time_point();
    stopped_at = std::chrono::steady_clock::time_point();
    timeScale = 1.0f;
}

Timer::~Timer()
{
}

void Timer::Start()
{
    started_at = std::chrono::steady_clock::now();
    currentState = TimerState::RUNNING;
}

void Timer::Resume()
{
    if (currentState == TimerState::PAUSED) {
        auto resume_time = std::chrono::steady_clock::now();
        auto pause_duration = resume_time - stopped_at;
        started_at += pause_duration;
        currentState = TimerState::RUNNING;
    }
}

void Timer::Pause()
{
    if (currentState == TimerState::RUNNING) {
        stopped_at = std::chrono::steady_clock::now();
        currentState = TimerState::PAUSED;
    }
}

void Timer::Stop()
{
    if (currentState != TimerState::STOPPED)
    {
        stopped_at = std::chrono::steady_clock::now(); // Record stop time
        currentState = TimerState::STOPPED;
    }
}

float Timer::ReadMS() const
{
    if (currentState == TimerState::RUNNING) {
        auto elapsed = std::chrono::steady_clock::now() - started_at;
        return duration_cast<std::chrono::milliseconds>(elapsed).count() * timeScale;
    }
    else {
        auto elapsed = stopped_at - started_at;
        return duration_cast<std::chrono::milliseconds>(elapsed).count() * timeScale;
    }
}

float Timer::ReadSec() const
{
    if (currentState == TimerState::RUNNING) {
        std::chrono::duration<float> elapsed = std::chrono::steady_clock::now() - started_at;
        return elapsed.count() * timeScale;
    }
    else {
        std::chrono::duration<float> elapsed = stopped_at - started_at;
        return elapsed.count() * timeScale;
    }
}

void Timer::StepFrame(float dt)
{
    if (currentState == TimerState::PAUSED) {
        auto step = std::chrono::milliseconds(static_cast<int>(1.0f / dt));
        stopped_at += step;
    }
}

void Timer::SetTimeScale(float scale)
{
    timeScale = scale;
}

TimerState Timer::GetState() const
{
    return currentState;
}