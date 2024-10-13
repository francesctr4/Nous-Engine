#include "Timer.h"

#include "SDL2.h"

// ---------------------------------------------
Timer::Timer()
{
	currentState = TimerState::STOPPED;
	started_at = 0;
	stopped_at = 0;

	timeScale = 1.0f; // Default time scale
}

Timer::~Timer()
{

}

// ---------------------------------------------
void Timer::Start()
{
	currentState = TimerState::RUNNING;
	started_at = SDL_GetTicks();
}

// ---------------------------------------------
void Timer::Resume()
{
	if (currentState == TimerState::PAUSED) {
	
		// Update started_at to adjust for the time spent in the paused state
		started_at += SDL_GetTicks() - stopped_at;

		currentState = TimerState::RUNNING;
	}

}

// ---------------------------------------------
void Timer::Pause()
{
	if (currentState == TimerState::RUNNING) {

		stopped_at = SDL_GetTicks();

		currentState = TimerState::PAUSED;
	}
}

// ---------------------------------------------

void Timer::Stop()
{
	started_at = 0;
	stopped_at = 0;
	currentState = TimerState::STOPPED;
}

// ---------------------------------------------
float Timer::ReadMS() const
{
	if (currentState == TimerState::RUNNING)
	{
		return static_cast<float>((SDL_GetTicks() - started_at) * timeScale);
	}
	else
	{
		return static_cast<float>((stopped_at - started_at) * timeScale);
	}
}

// ---------------------------------------------
float Timer::ReadSec() const
{
	if (currentState == TimerState::RUNNING)
	{
		return (SDL_GetTicks() - started_at) * 0.001f * timeScale;
	}
	else
	{
		return (stopped_at - started_at) * 0.001f * timeScale;
	}
}

// ---------------------------------------------
void Timer::StepFrame(float dt)
{
	if (currentState == TimerState::PAUSED) {

		stopped_at += (1 / dt);
	}

}

// ---------------------------------------------
void Timer::SetTimeScale(float scale)
{
	timeScale = scale;
}

TimerState Timer::GetState() const
{
	return currentState;
}
