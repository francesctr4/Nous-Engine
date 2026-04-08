#include "Engine/NOUS_Multithreading/NOUS_ThreadPool/include/NOUS_ThreadPool.h"
#include "Engine/NOUS_Multithreading/NOUS_Job/include/NOUS_Job.h"
#include "Engine/NOUS_Multithreading/NOUS_Thread/include/NOUS_Thread.h"

#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/Core/Logger/Logger.h"

#ifdef _PROFILING
#include <tracy/Tracy.hpp>
#endif

constexpr auto CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_MULTITHREADING;

/// @brief NOUS_ThreadPool constructor.
/// @note Marked explicit to prevent implicit conversions and copy-initialization from a single argument.
NOUS_Multithreading::NOUS_ThreadPool::NOUS_ThreadPool(uint8_t numThreads) :
	mShutdown(false)
{
	numThreads = std::max<uint8_t>(0, numThreads);
	mThreads.reserve(numThreads);

	for (uint8_t i = 0; i < numThreads; ++i)
	{
		mThreads.push_back(NOUS_NEW<NOUS_Thread>(MemoryTag::THREAD));

		mThreads[i]->Start([this, i]
		{
			mThreads[i]->SetName("Worker Thread " + std::to_string(i + 1));
			WorkerLoop(mThreads[i]);
			});
	}
}

/// @brief NOUS_ThreadPool destructor.
NOUS_Multithreading::NOUS_ThreadPool::~NOUS_ThreadPool()
{
	Shutdown();
}

/// @brief Adds a job to the queue and notifies a worker.
/// @param job The job to be executed.
void NOUS_Multithreading::NOUS_ThreadPool::SubmitJob(NOUS_Job* job)
{
	{
		std::lock_guard lock(mMutex);
		mJobQueue.push(job);
	}
	mConditionVar.notify_one();
}

/// @brief Deletes pending jobs, joins all threads and cleans up resources afterwards.
void NOUS_Multithreading::NOUS_ThreadPool::Shutdown()
{
	if (mShutdown.exchange(true))
	{
		return;
	}

	while (!mJobQueue.empty())
	{
		NOUS_Job* job = mJobQueue.front();
		NOUS_DELETE(job, MemoryTag::THREAD);
		mJobQueue.pop();
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
const std::vector<NOUS_Multithreading::NOUS_Thread*>& NOUS_Multithreading::NOUS_ThreadPool::GetThreads() const
{ 
	return mThreads; 
}

/// @return A queue of NOUS_Job to be executed by the thread pool.
const std::queue<NOUS_Multithreading::NOUS_Job*>& NOUS_Multithreading::NOUS_ThreadPool::GetJobQueue() const
{
	return mJobQueue;
}

/// @brief Worker loop that each thread executes to process jobs from the queue.
/// @param thread The thread executing this loop.
void NOUS_Multithreading::NOUS_ThreadPool::WorkerLoop(NOUS_Thread* thread)
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
				   job->GetName().c_str(), thread->GetName().c_str(), thread->GetID());

		try
		{
			job->Execute();
		}
		catch (const std::exception& e)
		{
			NOUS_ERROR_C(CURRENT_CHANNEL, ("Job '" + job->GetName() + "' failed: " + e.what()).c_str());
		}

		NOUS_DEBUG_C(CURRENT_CHANNEL, "Job '%s' completed successfully on thread '%s' (%u) in %.3f s",
				   job->GetName().c_str(), thread->GetName().c_str(),
				   thread->GetID(), thread->GetExecutionTimeMS() / 1000.0f);

		NOUS_DELETE(job, MemoryTag::THREAD);

		thread->StopExecutionTimer();
		thread->SetCurrentJob(nullptr);
		thread->SetThreadState(ThreadState::READY);
	}

	thread->SetThreadState(ThreadState::READY);
}
