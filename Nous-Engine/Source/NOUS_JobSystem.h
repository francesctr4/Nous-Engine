#pragma once

#include "Globals.h"

#include <functional>

#include "NOUS_ThreadPool.h"

namespace NOUS_Multithreading
{
	///////////////////////////////////////////////////////////////////////////
	/// @brief Maximum hardware threads available, minus one reserved for the main thread.
	///////////////////////////////////////////////////////////////////////////
	const uint8 c_MAX_HARDWARE_THREADS = []()
		{
			const unsigned int hardwareThreads = std::thread::hardware_concurrency();
			return (hardwareThreads == 0) ? 0 : (hardwareThreads - 1);
		}();

	///////////////////////////////////////////////////////////////////////////
	/// @brief High-level interface for job submission and management.
	///////////////////////////////////////////////////////////////////////////
	class NOUS_JobSystem
	{
	public:

		/// @brief NOUS_JobSystem constructor.
		/// @param size: Number of worker threads available inside the thread pool.
		/// @note If size is not specified, c_MAX_HARDWARE_THREADS is used.
		NOUS_JobSystem(const uint8 size = c_MAX_HARDWARE_THREADS);

		/// @brief NOUS_JobSystem destructor.
		/// @note Wait until all threads have finished their work and then delete the thread pool.
		~NOUS_JobSystem();

		/// @brief Submits a job to the thread pool, to be executed by a free worker thread.
		/// @note Job executes immediately if thread pool size is 0 (running on Main Thread).
		/// @param userJob: The function to execute.
		/// @param jobName: Optional name identifier.
		void SubmitJob(std::function<void()> userJob, const std::string& jobName = "Unnamed");

		/// @brief Blocks until all submitted jobs complete.
		void WaitForPendingJobs();

		/// @brief Resizes the thread pool to the specified number of threads.
		/// @param newSize: The new number of worker threads in the pool.
		/// @note If the size passed is 0, the program becomes single-threaded.
		/// @note Ensures all current jobs finish before resizing.
		void Resize(uint8 newSize);

		/// @return Reference to the underlying thread pool.
		const NOUS_ThreadPool& GetThreadPool() const;

		/// @return Number of pending unprocessed jobs.
		int GetPendingJobs() const;

	private:

		NOUS_ThreadPool*			mThreadPool;
		std::atomic<int>			mPendingJobs;

		std::mutex					mWaitMutex;
		std::condition_variable		mWaitCondition;

	};
}