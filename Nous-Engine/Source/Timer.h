#pragma once

#include <chrono>

enum class TimerState
{
	STOPPED,
	RUNNING,
	PAUSED
};

class Timer
{
public:

	Timer();
	virtual ~Timer();

	void Start();
	void Resume();
	void Pause();
	void Stop();

	void StepFrame(float dt);
	void SetTimeScale(float scale);

	float ReadMS() const;
	float ReadSec() const;

	TimerState GetState() const;

private:

	TimerState currentState;

	std::chrono::steady_clock::time_point started_at;
	std::chrono::steady_clock::time_point stopped_at;

	float timeScale;

};