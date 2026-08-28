#pragma once

#include <EngineCore/EngineExport.h>

#include <vector>
#include <queue>
#include <mutex>
#include <string>
#include <condition_variable>

namespace nous::engine::multithreading
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

		/// @return Names of the pending jobs, copied under the internal lock.
		/// @note Returns names rather than NOUS_Job* on purpose. Handing out pointers
		///       made this "snapshot" safe only until the lock was released: a worker
		///       pops and NOUS_DELETEs a job immediately afterwards, so the debug UI
		///       that read job->GetName() from the copy was a use-after-free.
		///       Copying the strings under the lock removes the hazard entirely.
		NOUS_ENGINE_API std::vector<std::string> GetJobQueueSnapshot() const;

	private:

		/// @brief Worker loop that each thread executes to process jobs from the queue.
		/// @param thread The thread executing this loop.
		void WorkerLoop(NOUS_Thread* thread);

		std::queue<NOUS_Job*>		mJobQueue;
		std::vector<NOUS_Thread*>	mThreads;

		mutable std::mutex			mMutex;
		std::condition_variable		mConditionVar;
		std::atomic<bool>			mShutdown;

	};
}