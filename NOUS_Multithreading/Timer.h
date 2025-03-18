#pragma once

#include "Globals.h"

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

	float started_at;
	float stopped_at;

	float timeScale;

};