#pragma once

#include "Globals.h"

#include <vector>
#include <queue>
#include <mutex>

#include "NOUS_Job.h"
#include "NOUS_Thread.h"

namespace NOUS_Multithreading
{
	///////////////////////////////////////////////////////////////////////////
	/// @brief Manages a pool of worker threads and job distribution between them.
	///////////////////////////////////////////////////////////////////////////
	class NOUS_ThreadPool
	{
	public:

		/// @brief NOUS_ThreadPool constructor.
		/// @note Marked explicit to prevent implicit conversions and copy-initialization from a single argument.
		explicit NOUS_ThreadPool(uint8 numThreads);

		/// @brief NOUS_ThreadPool destructor.
		~NOUS_ThreadPool();

		/// @brief Adds a job to the queue and notifies a worker.
		/// @param job The job to be executed.
		void SubmitJob(NOUS_Job* job);

		/// @brief Deletes pending jobs, joins all threads and cleans up resources afterwards.
		void Shutdown();

		/// @return A vector of NOUS_Thread contained inside the thread pool.
		const std::vector<NOUS_Thread*>& GetThreads() const;

	private:

		/// @brief Worker loop that each thread executes to process jobs from the queue.
		/// @param thread The thread executing this loop.
		void WorkerLoop(NOUS_Thread* thread);

		std::queue<NOUS_Job*>		mJobQueue;
		std::vector<NOUS_Thread*>	mThreads;

		std::mutex					mMutex;
		std::condition_variable		mConditionVar;
		std::atomic<bool>			mShutdown;

	};
}