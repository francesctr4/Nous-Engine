#include <NOUS_Multithreading/NOUS_ThreadPool.h>
#include <NOUS_Multithreading/NOUS_Job.h>
#include <NOUS_Multithreading/NOUS_Thread.h>

#include <MemoryManager/MemoryManager.h>
#include <Logger/Logger.h>

#ifdef _PROFILING
#include <tracy/Tracy.hpp>
#endif

constexpr auto CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_MULTITHREADING;

/// @brief NOUS_ThreadPool constructor.
/// @note Marked explicit to prevent implicit conversions and copy-initialization from a single argument.
nous::engine::multithreading::NOUS_ThreadPool::NOUS_ThreadPool(uint8_t numThreads) :
	mShutdown(false)
{
	mThreads.reserve(numThreads);

	for (uint8_t i = 0; i < numThreads; ++i)
	{
		mThreads.push_back(NOUS_NEW<NOUS_Thread>(MemoryTag::THREAD));
		mThreads[i]->SetName("Worker Thread " + std::to_string(i + 1));

		mThreads[i]->Start([this, i]
		{
			WorkerLoop(mThreads[i]);
			});
	}
}

/// @brief NOUS_ThreadPool destructor.
nous::engine::multithreading::NOUS_ThreadPool::~NOUS_ThreadPool()
{
	Shutdown();
}

/// @brief Adds a job to the queue and notifies a worker.
/// @param job The job to be executed.
void nous::engine::multithreading::NOUS_ThreadPool::SubmitJob(NOUS_Job* job)
{
	{
		std::lock_guard lock(mMutex);
		mJobQueue.push(job);
	}
	mConditionVar.notify_one();
}

/// @brief Deletes pending jobs, joins all threads and cleans up resources afterwards.
void nous::engine::multithreading::NOUS_ThreadPool::Shutdown()
{
	if (mShutdown.exchange(true))
	{
		return;
	}

	{
		std::lock_guard lock(mMutex);
		while (!mJobQueue.empty())
		{
			NOUS_Job* job = mJobQueue.front();
			NOUS_DELETE(job, MemoryTag::THREAD);
			mJobQueue.pop();
		}
	}

	mConditionVar.notify_all();

	for (NOUS_Thread* thread : mThreads)
	{
		thread->Join();
		NOUS_DELETE(thread, MemoryTag::THREAD);
	}

	mThreads.clear();
}

/// @return A vector of NOUS_Thread contained inside the thread pool.
const std::vector<nous::engine::multithreading::NOUS_Thread*>& nous::engine::multithreading::NOUS_ThreadPool::GetThreads() const
{ 
	return mThreads; 
}

/// @return Names of the pending jobs, copied under the internal lock.
std::vector<std::string> nous::engine::multithreading::NOUS_ThreadPool::GetJobQueueSnapshot() const
{
	std::lock_guard lock(mMutex);

	// The names are copied while the lock is held, so the caller never holds a
	// pointer into a job the pool may free the moment this returns.
	std::vector<std::string> names;
	names.reserve(mJobQueue.size());

	std::queue<NOUS_Job*> pending = mJobQueue;  // std::queue is not iterable
	while (!pending.empty())
	{
		if (const NOUS_Job* job = pending.front())
			names.emplace_back(job->GetName());

		pending.pop();
	}

	return names;
}

/// @brief Worker loop that each thread executes to process jobs from the queue.
/// @param thread The thread executing this loop.
void nous::engine::multithreading::NOUS_ThreadPool::WorkerLoop(NOUS_Thread* thread)
{
#ifdef _PROFILING
	tracy::SetThreadName(thread->GetName().c_str()); // Set thread name
#endif

	while (true)
	{
#ifdef _PROFILING
		ZoneScoped;
#endif

		NOUS_Job* job = nullptr;

		{
			std::unique_lock lock(mMutex);

			thread->SetThreadState(ThreadState::READY);

			mConditionVar.wait(lock, [this]
			{
				return !mJobQueue.empty() || mShutdown; // Threads sleep when there's no work.
				});

			if (mShutdown && mJobQueue.empty()) break;

			job = mJobQueue.front();
			mJobQueue.pop();
		}

		thread->SetThreadState(ThreadState::RUNNING);
		thread->SetCurrentJob(job);
		thread->StartExecutionTimer();

		NOUS_DEBUG_C(CURRENT_CHANNEL, "Executing job '%s' on thread '%s' (%u)",
				   job->GetName().c_str(), thread->GetName().c_str(), NOUS_Thread::GetDisplayID(thread->GetID()));

		try
		{
			job->Execute();
		}
		catch (const std::exception& e)
		{
			NOUS_ERROR_C(CURRENT_CHANNEL, ("Job '" + job->GetName() + "' failed: " + e.what()).c_str());
		}

		thread->StopExecutionTimer();

		NOUS_DEBUG_C(CURRENT_CHANNEL, "Job '%s' completed successfully on thread '%s' (%u) in %.3f s",
				   job->GetName().c_str(), thread->GetName().c_str(),
				   NOUS_Thread::GetDisplayID(thread->GetID()), thread->GetExecutionTimeMS() / 1000.0f);

		// Clear the pointer BEFORE freeing the job. The reverse order left a window
		// where GetCurrentJob() handed out a dangling pointer to whoever was
		// observing this thread.
		thread->SetCurrentJob(nullptr);
		NOUS_DELETE(job, MemoryTag::THREAD);
		thread->SetThreadState(ThreadState::READY);
	}

	thread->SetThreadState(ThreadState::READY);
}
