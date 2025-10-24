#ifndef TIMER_H
#define TIMER_H

#include "Engine/EngineExport.h"
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

	NOUS_ENGINE_API Timer();
	NOUS_ENGINE_API virtual ~Timer();

	NOUS_ENGINE_API void Start();
	NOUS_ENGINE_API void Resume();
	NOUS_ENGINE_API void Pause();
	NOUS_ENGINE_API void Stop();

	NOUS_ENGINE_API void StepFrame(float dt);
	NOUS_ENGINE_API void SetTimeScale(float scale);

	NOUS_ENGINE_API float ReadMS() const;
	NOUS_ENGINE_API float ReadSec() const;

	NOUS_ENGINE_API TimerState GetState() const;

private:

	TimerState currentState;

	std::chrono::steady_clock::time_point started_at;
	std::chrono::steady_clock::time_point stopped_at;

	float timeScale;

};

#endif // TIMER_H