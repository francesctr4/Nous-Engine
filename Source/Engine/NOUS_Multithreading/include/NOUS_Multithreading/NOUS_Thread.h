#pragma once

#include <EngineCore/EngineExport.h>

#include <atomic>
#include <thread>
#include <functional>
#include <mutex>
#include <string>
#include <chrono>

namespace nous::engine::multithreading
{
    // Forward declarations
	class NOUS_Job;

	///////////////////////////////////////////////////////////////////////////
	/// @brief Available thread states during its lifecycle.
	///////////////////////////////////////////////////////////////////////////
	enum class ThreadState : uint8_t
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
		NOUS_ENGINE_API void Start(const std::function<void()>& func);

		/// @brief Joins the thread if joinable.
		NOUS_ENGINE_API void Join();

		/// @brief Setters and getters.
		NOUS_ENGINE_API void SetName(const std::string& name);
		NOUS_ENGINE_API const std::string& GetName() const;
		NOUS_ENGINE_API void SetThreadState(ThreadState state);
		NOUS_ENGINE_API ThreadState GetThreadState() const;
		/// @brief Sets the job this thread is executing, and snapshots its name.
		/// @note Must be called from the thread that owns the job's lifetime.
		NOUS_ENGINE_API void SetCurrentJob(NOUS_Job* job);

		/// @return Raw pointer to the running job, or nullptr.
		/// @warning ONLY safe on the owning worker thread. The pool deletes the job
		///          as soon as it finishes, so any other thread that dereferences
		///          this pointer races that deletion — use GetCurrentJobName().
		///          Off-thread, treat the result as a null/non-null flag only.
		NOUS_ENGINE_API NOUS_Job* GetCurrentJob() const;

		/// @return Copy of the running job's name, or "" if idle.
		/// @note Thread-safe by value: observers (the editor) get a snapshot and
		///       never touch memory owned by the pool. Returning a reference here
		///       would reintroduce the use-after-free this exists to prevent.
		[[nodiscard]] NOUS_ENGINE_API std::string GetCurrentJobName() const;
		NOUS_ENGINE_API bool IsRunning() const;
		NOUS_ENGINE_API std::thread::id GetID() const;

		/// @brief Job execution time tracking.
		NOUS_ENGINE_API void StartExecutionTimer();
		NOUS_ENGINE_API void StopExecutionTimer();
		NOUS_ENGINE_API double GetExecutionTimeMS() const;

		/// @brief Sets a std::thread::id to this NOUS_Thread.
		/// @note Used mainly for registering main thread.
		NOUS_ENGINE_API void SetThreadID(std::thread::id id);

		/// @brief Returns a uint32_t derived from the thread ID hash, for display and logging only.
		/// @note Not suitable as a unique key — use std::thread::id directly for identity checks.
		NOUS_ENGINE_API static uint32_t GetDisplayID(std::thread::id id);

		/// @return std::string representation of the passed thread state.
		NOUS_ENGINE_API static std::string GetStringFromState(const ThreadState& state);

		/// @brief Sleep the current thread for an amount of time (ms).
		NOUS_ENGINE_API static void SleepMS(const uint32_t& ms);

	private:

		std::string					mThreadName;
		std::thread					mThreadHandle;
		std::thread::id				mThreadID;
		std::atomic<ThreadState>	mThreadState;

		std::atomic<bool>			mIsRunning;
		std::atomic<NOUS_Job*>		mCurrentJob;

		// The pointer above is atomic, but the *pointee* is not lifetime-safe: the
		// pool NOUS_DELETEs the job the moment it completes. Observers therefore
		// read this snapshot instead. ThreadSanitizer caught the editor doing
		// strlen() on a job name being freed by a worker (2026-08-22).
		mutable std::mutex			mCurrentJobNameMutex;
		std::string					mCurrentJobName;

        std::chrono::time_point<std::chrono::steady_clock>	mStartTime;
        std::chrono::time_point<std::chrono::steady_clock>	mEndTime;
        std::atomic<bool>									mTimerRunning;

	};
}