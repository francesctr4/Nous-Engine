#ifndef NOUS_MULTITHREADING_H
#define NOUS_MULTITHREADING_H

#include "Globals.h"
#include "Timer.h"
#include "MemoryManager.h"

#include <thread>
#include <functional>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>
#include <iostream>

namespace NOUS_Multithreading
{
	///////////////////////////////////////////////////////////////////////////
	/// @brief Maximum hardware threads available, minus one reserved for the main thread.
	///////////////////////////////////////////////////////////////////////////
	const uint32 c_MAX_HARDWARE_THREADS = (std::thread::hardware_concurrency() - 1);

	///////////////////////////////////////////////////////////////////////////
	/// @brief Represents an executable task with a name and function.
	///////////////////////////////////////////////////////////////////////////
	class NOUS_Job 
	{
	public:

		/// @brief NOUS_Job constructor.
		NOUS_Job(const std::string& name, std::function<void()> func) : 
			mName(name), mFunction(func) {}

		/// @brief Executes the stored function inside the job.
		void Execute() { mFunction(); }

		/// @return std::string with the NOUS_Job name identifier.
		const std::string& GetName() const { return mName; }

	private:

		std::string				mName;
		std::function<void()>	mFunction;

	};
}

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
		NOUS_Thread() : 
			mThreadID(0), mIsRunning(false), mCurrentJob(nullptr), mThreadState(ThreadState::READY) {}

		/// @brief NOUS_Thread destructor.
		~NOUS_Thread() { if (mIsRunning) Join(); }

		/// @brief NOUS_Thread move semantics definition.
		NOUS_Thread(NOUS_Thread&& other) noexcept = default;
		NOUS_Thread& operator=(NOUS_Thread&& other) noexcept = default;

		/// @brief NOUS_Thread delete copy operators.
		NOUS_Thread(const NOUS_Thread&) = delete;
		NOUS_Thread& operator=(const NOUS_Thread&) = delete;

		/// @brief Starts the thread with a given function.
		/// @param func The function to execute in the thread.
		void Start(std::function<void()> func) 
		{
			if (mIsRunning) return;

			mIsRunning = true;

			mThreadHandle = std::thread([this, func]() {
				func();
				mIsRunning = false;
			});

			mThreadID = GetThreadID(mThreadHandle.get_id());
			mThreadState.store(ThreadState::READY);
		}

		/// @brief Joins the thread if joinable.
		void Join() 
		{
			if (mThreadHandle.joinable())
			{
				mThreadHandle.join();
				mIsRunning = false;
			}
		}

		/// @brief Setters and getters.
		void SetName(const std::string& name) { mThreadName = name; }
		const std::string& GetName() const { return mThreadName; }
		void SetThreadState(ThreadState state) { mThreadState.store(state); }
		ThreadState GetThreadState() const { return mThreadState.load(); }
		void SetCurrentJob(NOUS_Job* job) { mCurrentJob = job; }
		NOUS_Job* GetCurrentJob() const { return mCurrentJob; }
		bool IsRunning() const { return mIsRunning; }
		uint32_t GetID() const { return mThreadID; }

		/// @brief Job execution time tracking.
		void StartExecutionTimer() { mExecutionTime.Start(); }
		void StopExecutionTimer() { mExecutionTime.Stop(); }
		double GetExecutionTimeMS() const { return mExecutionTime.ReadMS(); }

		/// @brief Sets a std::thread::id to this NOUS_Thread.
		/// @note Used mainly for registering main thread.
		void SetThreadID(std::thread::id id) 
		{
			std::stringstream ss;
			ss << id;
			mThreadID = static_cast<uint32>(std::stoul(ss.str()));
		}

		/// @brief Converts a std::thread::id to a numeric uint32.
		/// @note Relies on string conversion; platform-dependent.
		static uint32 GetThreadID(std::thread::id id)
		{
			std::stringstream ss;
			ss << id;
			return static_cast<uint32>(std::stoul(ss.str()));
		}

		/// @return std::string representation of the passed thread state.
		static const std::string GetStringFromState(const ThreadState& state)
		{
			switch (state)
			{
				case ThreadState::READY:	return "READY";
				case ThreadState::RUNNING:	return "RUNNING";
				default:					return "UNKNOWN";
			}
		}

		/// @brief Sleep the current thread for an amount of time (ms).
		static const void SleepMS(const uint32& ms)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(ms));
		}

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
		explicit NOUS_ThreadPool(size_t numThreads) : 
			mShutdown(false)
		{
			numThreads = std::max<size_t>(0, numThreads);
			mThreads.reserve(numThreads);

			for (size_t i = 0; i < numThreads; ++i) 
			{
				mThreads.push_back(NOUS_NEW<NOUS_Thread>(MemoryManager::MemoryTag::THREAD));

				mThreads[i]->Start([this, i]() {
					mThreads[i]->SetName("Worker Thread " + std::to_string(i + 1));
					WorkerLoop(mThreads[i]);
				});
			}
		}

		/// @brief NOUS_ThreadPool destructor.
		~NOUS_ThreadPool()
		{
			Shutdown();
		}

		/// @brief Adds a job to the queue and notifies a worker.
		/// @param job The job to be executed.
		void SubmitJob(NOUS_Job* job)
		{
			{
				std::lock_guard<std::mutex> lock(mMutex);
				mJobQueue.push(std::move(job));
			}
			mConditionVar.notify_one();
		}

		/// @brief Deletes pending jobs, joins all threads and cleans up resources afterwards.
		void Shutdown()
		{
			if (mShutdown.exchange(true)) 
			{
				return;
			}

			while (!mJobQueue.empty()) 
			{
				NOUS_Job* job = mJobQueue.front();
				NOUS_DELETE<NOUS_Job>(job, MemoryManager::MemoryTag::THREAD);
				mJobQueue.pop();
			}

			mConditionVar.notify_all();

			for (NOUS_Thread* thread : mThreads) 
			{
				thread->Join();
				NOUS_DELETE<NOUS_Thread>(thread, MemoryManager::MemoryTag::THREAD);
			}

			mThreads.clear();
		}

		/// @return A vector of NOUS_Thread contained inside the thread pool.
		const std::vector<NOUS_Thread*>& GetThreads() const { return mThreads; }

	private:

		/// @brief Worker loop that each thread executes to process jobs from the queue.
		/// @param thread The thread executing this loop.
		void WorkerLoop(NOUS_Thread* thread)
		{
			while (true)
			{
				NOUS_Job* job = nullptr;

				{
					std::unique_lock<std::mutex> lock(mMutex);

					thread->SetThreadState(ThreadState::READY);

					mConditionVar.wait(lock, [this]() {
						return !mJobQueue.empty() || mShutdown; // Threads sleep when there's no work.
					});

					if (mShutdown && mJobQueue.empty()) break;

					job = std::move(mJobQueue.front());
					mJobQueue.pop();
				}

				thread->SetThreadState(ThreadState::RUNNING);
				thread->SetCurrentJob(job);
				thread->StartExecutionTimer();

				try
				{
					job->Execute();
				}
				catch (const std::exception& e)
				{
					std::cerr << "Job '" << job->GetName() << "' failed: " << e.what() << '\n';
				}

				NOUS_DELETE<NOUS_Job>(job, MemoryManager::MemoryTag::THREAD);

				thread->StopExecutionTimer();
				thread->SetCurrentJob(nullptr);
				thread->SetThreadState(ThreadState::READY);
			}

			thread->SetThreadState(ThreadState::READY);
		}

		std::queue<NOUS_Job*>		mJobQueue;
		std::vector<NOUS_Thread*>	mThreads;

		std::mutex					mMutex;
		std::condition_variable		mConditionVar;
		std::atomic<bool>			mShutdown;

	};
}

namespace NOUS_Multithreading
{
	///////////////////////////////////////////////////////////////////////////
	/// @brief High-level interface for job submission and management.
	///////////////////////////////////////////////////////////////////////////
	class NOUS_JobSystem 
	{
	public:

		/// @brief NOUS_JobSystem constructor.
		/// @param size: Number of worker threads available inside the thread pool.
		/// @note If size is not specified, c_MAX_HARDWARE_THREADS is used.
		NOUS_JobSystem(const uint32 size = c_MAX_HARDWARE_THREADS)
		{
			mPendingJobs = 0;
			mThreadPool = NOUS_NEW<NOUS_ThreadPool>(MemoryManager::MemoryTag::THREAD, size);
		}	

		/// @brief NOUS_JobSystem destructor.
		/// @note Wait until all threads have finished their work and then delete the thread pool.
		~NOUS_JobSystem()
		{
			WaitForPendingJobs();

			NOUS_DELETE<NOUS_ThreadPool>(mThreadPool, MemoryManager::MemoryTag::THREAD);
		}

		/// @brief Submits a job to the thread pool, to be executed by a free worker thread.
		/// @note Job executes immediately if thread pool size is 0 (running on Main Thread).
		/// @param userJob: The function to execute.
		/// @param jobName: Optional name identifier.
		void SubmitJob(std::function<void()> userJob, const std::string& jobName = "Unnamed") 
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
		void WaitForPendingJobs() 
		{
			std::unique_lock<std::mutex> lock(mWaitMutex);
			mWaitCondition.wait(lock, [this]() { return mPendingJobs == 0; });
		}

		/// @brief Resizes the thread pool to the specified number of threads.
		/// @param newSize: The new number of worker threads in the pool.
		/// @note If the size passed is 0, the program becomes single-threaded.
		/// @note Ensures all current jobs finish before resizing.
		void Resize(uint32 newSize) 
		{
			WaitForPendingJobs(); 

			NOUS_DELETE<NOUS_ThreadPool>(mThreadPool, MemoryManager::MemoryTag::THREAD);
			mThreadPool = NOUS_NEW<NOUS_ThreadPool>(MemoryManager::MemoryTag::THREAD, newSize);
		}

		/// @return Reference to the underlying thread pool.
		const NOUS_ThreadPool& GetThreadPool() const { return *mThreadPool; }

		/// @return Number of pending unprocessed jobs.
		int GetPendingJobs() const { return mPendingJobs; }

	private:

		NOUS_ThreadPool*			mThreadPool;
		std::atomic<int>			mPendingJobs;

		std::mutex					mWaitMutex;
		std::condition_variable		mWaitCondition;

	};
}

namespace NOUS_Multithreading
{
	/// @brief Global pointer to the main thread instance.
	/// @note Initialized by RegisterMainThread() and cleaned up by UnregisterMainThread().
	static NOUS_Thread* sMainThread = nullptr;

	/// @brief Initializes the main thread tracking.
	/// @note Must be paired with UnregisterMainThread() to prevent memory leaks.
	static void RegisterMainThread()
	{
		if (!sMainThread) 
		{
			sMainThread = NOUS_NEW<NOUS_Thread>(MemoryManager::MemoryTag::THREAD);

			sMainThread->SetName("Main Thread");
			sMainThread->SetThreadID(std::this_thread::get_id());
			sMainThread->SetThreadState(ThreadState::RUNNING);
			sMainThread->StartExecutionTimer();
		}
	}

	/// @brief Deletes the main thread instance if it exists.
	/// @note Should be called during application shutdown.
	static void UnregisterMainThread()
	{
		if (sMainThread)
		{
			NOUS_DELETE<NOUS_Thread>(sMainThread, MemoryManager::MemoryTag::THREAD);
			sMainThread = nullptr;
		}
	}

	/// @brief Retrieves the main thread instance.
	/// @return Pointer to the main thread object, or nullptr if not registered.
	static NOUS_Thread* GetMainThread()
	{
		return sMainThread;
	}

	/// @brief Prints debugging information about the job system state.
	/// @param system: The job system instance to inspect.
	static void JobSystemDebugInfo(const NOUS_JobSystem& system)
	{
		const NOUS_ThreadPool& threadPool = system.GetThreadPool();
		const std::vector<NOUS_Thread*>& threads = threadPool.GetThreads();

		std::cout << "===== Job System Debug Info =====\n";
		std::cout << "Pending Jobs: " << system.GetPendingJobs() << "\n";

		// Main thread diagnostics
		NOUS_Thread* mainThread = GetMainThread();
		if (mainThread) 
		{
			std::cout << "Thread '" << mainThread->GetName() 
				      << "' (ID: " << mainThread->GetID() << ") - "
					  << NOUS_Thread::GetStringFromState(mainThread->GetThreadState())
					  << ", Exec Time: " << mainThread->GetExecutionTimeMS() << "ms" << "\n";
		}

		// Worker thread diagnostics
		for (NOUS_Thread* thread : threads)
		{
			std::cout << "Thread '" << thread->GetName() 
				      << "' (ID: " << thread->GetID() << ") - " 
					  << thread->GetStringFromState(thread->GetThreadState()).c_str()
					  << ", Exec Time: " << thread->GetExecutionTimeMS() << "ms"
					  << ", Job: " << (thread->GetCurrentJob() ? thread->GetCurrentJob()->GetName() : "None") << "\n";
		}

		// Sequential mode detection
		if (threads.empty())
		{
			std::cout << "[Sequential Mode] Jobs executed on main thread\n";
		}
	}
}

#endif // NOUS_MULTITHREADING_H