#ifndef NOUS_THREAD_POOL_H
#define NOUS_THREAD_POOL_H

#include "Engine/Core/EngineExport.h"

#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace NOUS_Multithreading
{
	// Forward declarations
	class NOUS_Job;
	class NOUS_Thread;

	///////////////////////////////////////////////////////////////////////////
	/// @brief Manages a pool of worker threads and job distribution between them.
	///////////////////////////////////////////////////////////////////////////
	class NOUS_ThreadPool
	{
	public:

		/// @brief NOUS_ThreadPool constructor.
		/// @note Marked explicit to prevent implicit conversions and copy-initialization from a single argument.
		NOUS_ENGINE_API explicit NOUS_ThreadPool(uint8_t numThreads);

		/// @brief NOUS_ThreadPool destructor.
		NOUS_ENGINE_API ~NOUS_ThreadPool();

		/// @brief Adds a job to the queue and notifies a worker.
		/// @param job The job to be executed.
		NOUS_ENGINE_API void SubmitJob(NOUS_Job* job);

		/// @brief Deletes pending jobs, joins all threads and cleans up resources afterwards.
		NOUS_ENGINE_API void Shutdown();

		/// @return A vector of NOUS_Thread contained inside the thread pool.
		NOUS_ENGINE_API const std::vector<NOUS_Thread*>& GetThreads() const;

		/// @return A queue of NOUS_Job to be executed by the thread pool.
		NOUS_ENGINE_API const std::queue<NOUS_Job*>& GetJobQueue() const;

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

#endif // NOUS_THREAD_POOL_H