#ifndef NOUSTHREAD_H
#define NOUSTHREAD_H

#include "Engine/Core/Globals.h"
#include "Engine/Systems/Time Management/Timer.h"
#include "Engine/Core/EngineExport.h"
#include "Engine/Multithreading/NOUS_Job/include/NOUS_Job.h"

#include <thread>
#include <functional>

namespace NOUS_Multithreading
{
	///////////////////////////////////////////////////////////////////////////
	/// @brief Available thread states during its lifecycle.
	///////////////////////////////////////////////////////////////////////////
	enum class ThreadState
	{
		READY = 0,		// Idle and waiting for a job.
		RUNNING = 1		// Actively executing a job.
	};

	///////////////////////////////////////////////////////////////////////////
	/// @brief std::thread wrapper with additional metadata (name, state, timer,...)
	///////////////////////////////////////////////////////////////////////////
	class NOUS_Thread
	{
	public:

		/// @brief NOUS_Thread constructor.
		NOUS_ENGINE_API NOUS_Thread();

		/// @brief NOUS_Thread destructor.
		NOUS_ENGINE_API ~NOUS_Thread();

		/// @brief NOUS_Thread move semantics definition.
		NOUS_Thread(NOUS_Thread&& other) noexcept = delete;
		NOUS_Thread& operator=(NOUS_Thread&& other) noexcept = delete;

		/// @brief NOUS_Thread delete copy operators.
		NOUS_Thread(const NOUS_Thread&) = delete;
		NOUS_Thread& operator=(const NOUS_Thread&) = delete;

		/// @brief Starts the thread with a given function.
		/// @param func The function to execute in the thread.
		NOUS_ENGINE_API void Start(std::function<void()> func);

		/// @brief Joins the thread if joinable.
		NOUS_ENGINE_API void Join();

		/// @brief Setters and getters.
		NOUS_ENGINE_API void SetName(const std::string& name);
		NOUS_ENGINE_API const std::string& GetName() const;
		NOUS_ENGINE_API void SetThreadState(ThreadState state);
		NOUS_ENGINE_API ThreadState GetThreadState() const;
		NOUS_ENGINE_API void SetCurrentJob(NOUS_Job* job);
		NOUS_ENGINE_API NOUS_Job* GetCurrentJob() const;
		NOUS_ENGINE_API bool IsRunning() const;
		NOUS_ENGINE_API uint32 GetID() const;

		/// @brief Job execution time tracking.
		NOUS_ENGINE_API void StartExecutionTimer();
		NOUS_ENGINE_API void StopExecutionTimer();
		NOUS_ENGINE_API double GetExecutionTimeMS() const;

		/// @brief Sets a std::thread::id to this NOUS_Thread.
		/// @note Used mainly for registering main thread.
		NOUS_ENGINE_API void SetThreadID(std::thread::id id);

		/// @brief Converts a std::thread::id to a numeric uint32.
		/// @note Relies on string conversion; platform-dependent.
		NOUS_ENGINE_API static uint32 GetThreadID(std::thread::id id);

		/// @return std::string representation of the passed thread state.
		NOUS_ENGINE_API static const std::string GetStringFromState(const ThreadState& state);

		/// @brief Sleep the current thread for an amount of time (ms).
		NOUS_ENGINE_API static const void SleepMS(const uint32& ms);

	private:

		std::string					mThreadName;
		std::thread					mThreadHandle;
		uint32						mThreadID;
		std::atomic<ThreadState>	mThreadState;

		std::atomic<bool>			mIsRunning;
		NOUS_Job*					mCurrentJob;
		Timer						mExecutionTime;

	};
}

#endif // NOUSTHREAD_H