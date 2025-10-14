#ifndef TIMER_H
#define TIMER_H

#include <Engine/Core/Export.h>
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

	NOUS_API Timer();
	NOUS_API virtual ~Timer();

	NOUS_API void Start();
	NOUS_API void Resume();
	NOUS_API void Pause();
	NOUS_API void Stop();

	NOUS_API void StepFrame(float dt);
	NOUS_API void SetTimeScale(float scale);

	NOUS_API float ReadMS() const;
	NOUS_API float ReadSec() const;

	NOUS_API TimerState GetState() const;

private:

	TimerState currentState;

	std::chrono::steady_clock::time_point started_at;
	std::chrono::steady_clock::time_point stopped_at;

	float timeScale;

};

#endif // TIMER_H