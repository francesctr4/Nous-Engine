#include "Engine/NOUS_Multithreading/NOUS_JobSystem/include/NOUS_JobSystem.h"
#include "Engine/NOUS_Multithreading/NOUS_Job/include/NOUS_Job.h"
#include "Engine/NOUS_Multithreading/NOUS_ThreadPool/include/NOUS_ThreadPool.h"

#include "Engine/Core/Logger/Logger.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"

constexpr auto CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_MULTITHREADING;

/// @brief NOUS_JobSystem constructor.
/// @param size: Number of worker threads available inside the thread pool.
/// @note If size is not specified, c_MAX_HARDWARE_THREADS is used.
nous::engine::multithreading::NOUS_JobSystem::NOUS_JobSystem(const uint8_t size)
{
	mPendingJobs = 0;
	mThreadPool = NOUS_NEW<NOUS_ThreadPool>(MemoryTag::THREAD, size);
}

/// @brief NOUS_JobSystem destructor.
/// @note Wait until all threads have finished their work and then delete the thread pool.
nous::engine::multithreading::NOUS_JobSystem::~NOUS_JobSystem()
{
	WaitForPendingJobs();

	NOUS_DELETE(mThreadPool, MemoryTag::THREAD);
}

/// @brief Submits a job to the thread pool, to be executed by a free worker thread.
/// @note Job executes immediately if thread pool size is 0 (running on Main Thread).
/// @param userJob: The function to execute.
/// @param jobName: Optional name identifier.
void nous::engine::multithreading::NOUS_JobSystem::SubmitJob(const std::function<void()>& userJob, const std::string& jobName)
{
	++mPendingJobs;

	std::function<void()> wrappedJob = [this, userJob]
	{
		userJob();

		if (mPendingJobs-- == 1)
		{
			mWaitCondition.notify_all();
		}

		};

	auto* job = NOUS_NEW<NOUS_Job>(MemoryTag::THREAD, jobName, wrappedJob);

	if (mThreadPool->GetThreads().empty()) // Running on Main Thread (sequentially)
	{
		job->Execute();
		NOUS_DELETE(job, MemoryTag::THREAD);
	}
	else
	{
		NOUS_DEBUG_C(CURRENT_CHANNEL, "Submitting job '%s' to thread pool (%d pending jobs)",
					 jobName.c_str(), GetPendingJobs());
		mThreadPool->SubmitJob(job);
	}
}

/// @brief Queues work to run on the MAIN thread, drained once per frame.
/// @param task: The function to execute on the main thread.
/// @param taskName: Optional name identifier, used in the discard warning.
void nous::engine::multithreading::NOUS_JobSystem::SubmitToMainThread(std::function<void()> task, const std::string& taskName)
{
	std::lock_guard lock(m_mainThreadMutex);

	if (m_mainThreadQueueClosed)
	{
		NOUS_WARN_C(CURRENT_CHANNEL, "SubmitToMainThread('%s') after shutdown - discarded.", taskName.c_str());
		return;
	}

	m_mainThreadQueue.push_back(MainThreadTask{ std::move(task), taskName });
}

/// @brief Runs and clears every queued main-thread task. Call ONLY from the main thread.
void nous::engine::multithreading::NOUS_JobSystem::DrainMainThreadQueue()
{
	// Swap the queue out under the lock and run OUTSIDE it. Running while holding
	// the mutex would deadlock the moment a task queues another task - which is a
	// legitimate thing to do, so it must work.
	std::vector<MainThreadTask> batch;
	{
		std::lock_guard lock(m_mainThreadMutex);
		batch.swap(m_mainThreadQueue);
	}

	for (auto& task : batch)
	{
		task.fn();
	}
}

/// @brief Stops accepting main-thread tasks and discards those still queued.
void nous::engine::multithreading::NOUS_JobSystem::CloseMainThreadQueue()
{
	std::lock_guard lock(m_mainThreadMutex);

	m_mainThreadQueueClosed = true;

	if (!m_mainThreadQueue.empty())
	{
		NOUS_WARN_C(CURRENT_CHANNEL, "Discarding %zu queued main-thread task(s) at shutdown.",
					m_mainThreadQueue.size());
	}

	m_mainThreadQueue.clear();
}

/// @return Number of main-thread tasks awaiting a drain.
size_t nous::engine::multithreading::NOUS_JobSystem::GetPendingMainThreadTaskCount() const
{
	std::lock_guard lock(m_mainThreadMutex);
	return m_mainThreadQueue.size();
}

/// @brief Blocks until all submitted jobs complete.
void nous::engine::multithreading::NOUS_JobSystem::WaitForPendingJobs()
{
	std::unique_lock lock(mWaitMutex);
	mWaitCondition.wait(lock, [this] { return mPendingJobs == 0; });
}

/// @brief Resizes the thread pool to the specified number of threads.
/// @param newSize: The new number of worker threads in the pool.
/// @note If the size passed is 0, the program becomes single-threaded.
/// @note Ensures all current jobs finish before resizing.
void nous::engine::multithreading::NOUS_JobSystem::Resize(uint8_t newSize)
{
	WaitForPendingJobs();

	NOUS_DELETE(mThreadPool, MemoryTag::THREAD);
	mThreadPool = NOUS_NEW<NOUS_ThreadPool>(MemoryTag::THREAD, newSize);
}

/// @return Reference to the underlying thread pool.
const nous::engine::multithreading::NOUS_ThreadPool& nous::engine::multithreading::NOUS_JobSystem::GetThreadPool() const
{ 
	return *mThreadPool; 
}

/// @return Number of pending unprocessed jobs.
int nous::engine::multithreading::NOUS_JobSystem::GetPendingJobs() const
{
	return mPendingJobs;
}

/// @return Number of worker threads in the pool.
size_t nous::engine::multithreading::NOUS_JobSystem::GetWorkerCount() const
{
	return mThreadPool->GetThreads().size();
}
