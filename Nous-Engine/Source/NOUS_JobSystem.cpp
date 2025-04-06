#include "NOUS_JobSystem.h"

#include "NOUS_Job.h"

#include "MemoryManager.h"

/// @brief NOUS_JobSystem constructor.
/// @param size: Number of worker threads available inside the thread pool.
/// @note If size is not specified, c_MAX_HARDWARE_THREADS is used.
NOUS_Multithreading::NOUS_JobSystem::NOUS_JobSystem(const uint32 size)
{
	mPendingJobs = 0;
	mThreadPool = NOUS_NEW<NOUS_ThreadPool>(MemoryManager::MemoryTag::THREAD, size);
}

/// @brief NOUS_JobSystem destructor.
/// @note Wait until all threads have finished their work and then delete the thread pool.
NOUS_Multithreading::NOUS_JobSystem::~NOUS_JobSystem()
{
	WaitForPendingJobs();

	NOUS_DELETE<NOUS_ThreadPool>(mThreadPool, MemoryManager::MemoryTag::THREAD);
}

/// @brief Submits a job to the thread pool, to be executed by a free worker thread.
/// @note Job executes immediately if thread pool size is 0 (running on Main Thread).
/// @param userJob: The function to execute.
/// @param jobName: Optional name identifier.
void NOUS_Multithreading::NOUS_JobSystem::SubmitJob(std::function<void()> userJob, const std::string& jobName)
{
	mPendingJobs++;

	std::function<void()> wrappedJob = [this, userJob]() {

		userJob();

		if (mPendingJobs-- == 1)
		{
			mWaitCondition.notify_all();
		}

		};

	NOUS_Job* job = NOUS_NEW<NOUS_Job>(MemoryManager::MemoryTag::THREAD, jobName, wrappedJob);

	if (mThreadPool->GetThreads().empty()) // Running on Main Thread (sequentially)
	{
		job->Execute();
		NOUS_DELETE<NOUS_Job>(job, MemoryManager::MemoryTag::THREAD);
	}
	else
	{
		mThreadPool->SubmitJob(job);
	}
}

/// @brief Blocks until all submitted jobs complete.
void NOUS_Multithreading::NOUS_JobSystem::WaitForPendingJobs()
{
	std::unique_lock<std::mutex> lock(mWaitMutex);
	mWaitCondition.wait(lock, [this]() { return mPendingJobs == 0; });
}

/// @brief Resizes the thread pool to the specified number of threads.
/// @param newSize: The new number of worker threads in the pool.
/// @note If the size passed is 0, the program becomes single-threaded.
/// @note Ensures all current jobs finish before resizing.
void NOUS_Multithreading::NOUS_JobSystem::Resize(uint32 newSize)
{
	WaitForPendingJobs();

	NOUS_DELETE<NOUS_ThreadPool>(mThreadPool, MemoryManager::MemoryTag::THREAD);
	mThreadPool = NOUS_NEW<NOUS_ThreadPool>(MemoryManager::MemoryTag::THREAD, newSize);
}

/// @return Reference to the underlying thread pool.
const NOUS_Multithreading::NOUS_ThreadPool& NOUS_Multithreading::NOUS_JobSystem::GetThreadPool() const 
{ 
	return *mThreadPool; 
}

/// @return Number of pending unprocessed jobs.
int NOUS_Multithreading::NOUS_JobSystem::GetPendingJobs() const 
{ 
	return mPendingJobs; 
}
