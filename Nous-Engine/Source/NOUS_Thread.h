#pragma once

#include "Globals.h"
#include "Timer.h"

#include "NOUS_Job.h"

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
		NOUS_Thread();

		/// @brief NOUS_Thread destructor.
		~NOUS_Thread();

		/// @brief NOUS_Thread move semantics definition.
		NOUS_Thread(NOUS_Thread&& other) noexcept = default;
		NOUS_Thread& operator=(NOUS_Thread&& other) noexcept = default;

		/// @brief NOUS_Thread delete copy operators.
		NOUS_Thread(const NOUS_Thread&) = delete;
		NOUS_Thread& operator=(const NOUS_Thread&) = delete;

		/// @brief Starts the thread with a given function.
		/// @param func The function to execute in the thread.
		void Start(std::function<void()> func);

		/// @brief Joins the thread if joinable.
		void Join();

		/// @brief Setters and getters.
		void SetName(const std::string& name);
		const std::string& GetName() const;
		void SetThreadState(ThreadState state);
		ThreadState GetThreadState() const;
		void SetCurrentJob(NOUS_Job* job);
		NOUS_Job* GetCurrentJob() const;
		bool IsRunning() const;
		uint32 GetID() const;

		/// @brief Job execution time tracking.
		void StartExecutionTimer();
		void StopExecutionTimer();
		double GetExecutionTimeMS() const;

		/// @brief Sets a std::thread::id to this NOUS_Thread.
		/// @note Used mainly for registering main thread.
		void SetThreadID(std::thread::id id);

		/// @brief Converts a std::thread::id to a numeric uint32.
		/// @note Relies on string conversion; platform-dependent.
		static uint32 GetThreadID(std::thread::id id);

		/// @return std::string representation of the passed thread state.
		static const std::string GetStringFromState(const ThreadState& state);

		/// @brief Sleep the current thread for an amount of time (ms).
		static const void SleepMS(const uint32& ms);

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