#pragma once

#include "Globals.h"

#include <thread>
#include <functional>
#include <atomic>
#include <sstream>
#include <mutex>
#include <future>
#include <unordered_set>
#include <numeric>

#include "Timer.h"
#include <queue>
#include "MemoryManager.h"

namespace NOUS_Multithreading
{
	const uint32 c_MAX_HARDWARE_THREADS = std::thread::hardware_concurrency();
}

namespace NOUS_Multithreading
{
	// In NOUS_Multithreading.h, within the NOUS_Multithreading namespace
	class NOUS_Job {
	public:
		NOUS_Job(const std::string& name, std::function<void()> func)
			: mName(name), mFunction(func) {}

		void Execute() { mFunction(); }
		const std::string& GetName() const { return mName; }

	private:
		std::string mName;
		std::function<void()> mFunction;
	};
}

namespace NOUS_Multithreading
{
	enum class ThreadState 
	{
		READY = 0,
		RUNNING = 1,
		WAITING = 2
	};

	class NOUS_Thread 
	{
	public:

		NOUS_Thread() : mThreadID(0), mIsRunning(false), mCurrentJob(nullptr), mThreadState(ThreadState::READY) {}
		~NOUS_Thread() { if (mIsRunning) Join(); }

		NOUS_Thread(NOUS_Thread&& other) noexcept :
			mThreadHandle(std::move(other.mThreadHandle)),
			mIsRunning(other.mIsRunning.load()),
			mCurrentJob(std::move(other.mCurrentJob)),
			mThreadName(std::move(other.mThreadName)),
			mThreadID(other.mThreadID),
			mThreadState(other.mThreadState.load()),
			mExecutionTime(std::move(other.mExecutionTime))
		{}

		NOUS_Thread& operator=(NOUS_Thread&& other) noexcept {
			if (this != &other) {
				mThreadHandle = std::move(other.mThreadHandle);
				mIsRunning.store(other.mIsRunning.load());
				mCurrentJob = std::move(other.mCurrentJob);
				mThreadName = std::move(other.mThreadName);
				mThreadID = other.mThreadID;
				mThreadState.store(other.mThreadState.load());
				mExecutionTime = std::move(other.mExecutionTime);
			}
			return *this;
		}

		// Delete copy operations
		NOUS_Thread(const NOUS_Thread&) = delete;
		NOUS_Thread& operator=(const NOUS_Thread&) = delete;

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

		void Join() 
		{
			if (mThreadHandle.joinable())
			{
				mThreadHandle.join();
				mIsRunning = false;
			}
		}

		bool IsRunning() const { return mIsRunning; }

		// Thread metadata and state management
		void SetName(const std::string& name) { mThreadName = name; }
		const std::string& GetName() const { return mThreadName; }

		uint32_t GetID() const { return mThreadID; }

		void SetThreadState(ThreadState state) { mThreadState.store(state); }
		ThreadState GetThreadState() const { return mThreadState.load(); }

		void SetCurrentJob(NOUS_Job* job) { mCurrentJob = job; }
		NOUS_Job* GetCurrentJob() const { return mCurrentJob; }

		void StartExecutionTimer() { mExecutionTime.Start(); }
		void StopExecutionTimer() { mExecutionTime.Stop(); }
		double GetExecutionTimeMS() const { return mExecutionTime.ReadMS(); }

		static uint32 GetThreadID(std::thread::id id)
		{
			std::stringstream ss;
			ss << id;
			return static_cast<uint32>(std::stoul(ss.str()));
		}

		static const std::string& GetStringFromState(const ThreadState& state)
		{
			return stateToString.at(state);
		}

	private:

		static const std::unordered_map<NOUS_Multithreading::ThreadState, std::string> stateToString;

		std::thread mThreadHandle;
		std::atomic<bool> mIsRunning;

		NOUS_Job* mCurrentJob;
		std::string mThreadName;
		uint32 mThreadID;
		std::atomic<ThreadState> mThreadState;
		Timer mExecutionTime;

	};
}

namespace NOUS_Multithreading
{
	class NOUS_ThreadPool
	{
	public:

		explicit NOUS_ThreadPool(size_t numThreads) : mShutdown(false)
		{
			numThreads = std::max<size_t>(0, numThreads);

			mThreads.reserve(numThreads);
			for (size_t i = 0; i < numThreads; ++i) {
				mThreads.push_back(NOUS_NEW<NOUS_Thread>(MemoryManager::MemoryTag::THREAD));
				mThreads[i]->Start([this, i]() {
					mThreads[i]->SetName("WorkerThread_" + std::to_string(i));
					WorkerLoop(mThreads[i]);
					});
			}
		}

		~NOUS_ThreadPool()
		{
			Shutdown();
		}

		void SubmitJob(NOUS_Job* job)
		{
			{
				std::lock_guard<std::mutex> lock(mMutex);
				mJobQueue.push(std::move(job));
			}
			mCondition.notify_one();
		}

		void Shutdown()
		{
			// Atomically set shutdown flag and check previous value
			if (mShutdown.exchange(true)) {
				return;
			}

			// Delete all pending jobs in the queue
			while (!mJobQueue.empty()) 
			{
				NOUS_Job* job = mJobQueue.front();
				NOUS_DELETE<NOUS_Job>(job, MemoryManager::MemoryTag::THREAD);
				mJobQueue.pop();
			}

			mCondition.notify_all();

			// Join all threads first
			for (auto* thread : mThreads) 
			{
				thread->Join();
			}

			// Delete all thread objects
			for (auto* thread : mThreads) 
			{
				NOUS_DELETE<NOUS_Thread>(thread, MemoryManager::MemoryTag::THREAD);
			}
			mThreads.clear();
		}

		const std::vector<NOUS_Thread*>& GetThreads() const { return mThreads; }

	private:

		void WorkerLoop(NOUS_Thread* thread)
		{
			while (true)
			{
				NOUS_Job* job;

				{
					std::unique_lock<std::mutex> lock(mMutex);

					thread->SetThreadState(ThreadState::READY);

					mCondition.wait(lock, [this]() {
						return !mJobQueue.empty() || mShutdown;
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
					std::cerr << "Job failed: " << e.what() << '\n';
				}

				NOUS_DELETE<NOUS_Job>(job, MemoryManager::MemoryTag::THREAD);

				thread->SetCurrentJob(nullptr);
				thread->SetThreadState(ThreadState::READY);
				thread->StopExecutionTimer();
			}

			thread->SetThreadState(ThreadState::READY);
		}

		std::vector<NOUS_Thread*> mThreads; // Changed to raw pointers
		std::queue<NOUS_Job*> mJobQueue;
		std::mutex mMutex;
		std::condition_variable mCondition;
		std::atomic<bool> mShutdown;
	};
}

namespace NOUS_Multithreading
{
	class NOUS_JobSystem 
	{
	public:

		NOUS_JobSystem(const uint32 size = c_MAX_HARDWARE_THREADS)
		{
			mThreadPool = NOUS_NEW<NOUS_ThreadPool>(MemoryManager::MemoryTag::THREAD, size);
		}

		~NOUS_JobSystem()
		{
			WaitForAll();

			NOUS_DELETE<NOUS_ThreadPool>(mThreadPool, MemoryManager::MemoryTag::THREAD);
		}

		void SubmitJob(std::function<void()> userJob, const std::string& jobName = "Unnamed") 
		{
			mPendingJobs++;

			auto wrappedJob = [this, userJob]() {
				userJob();
				if (mPendingJobs-- == 1) {
					mWaitCondition.notify_all();
				}
				};

			auto job = NOUS_NEW<NOUS_Job>(MemoryManager::MemoryTag::THREAD, jobName, wrappedJob);

			if (mThreadPool->GetThreads().empty()) {
				job->Execute();
				NOUS_DELETE<NOUS_Job>(job, MemoryManager::MemoryTag::THREAD);
			}
			else {
				mThreadPool->SubmitJob(job);
			}
		}

		void WaitForAll() 
		{
			std::unique_lock<std::mutex> lock(mWaitMutex);
			mWaitCondition.wait(lock, [this]() { return mPendingJobs == 0; });
		}

		void Resize(uint32 newSize) 
		{
			WaitForAll(); // Ensure all current jobs finish
			NOUS_DELETE<NOUS_ThreadPool>(mThreadPool, MemoryManager::MemoryTag::THREAD);
			mThreadPool = NOUS_NEW<NOUS_ThreadPool>(MemoryManager::MemoryTag::THREAD, newSize);
		}

		const NOUS_ThreadPool& GetThreadPool() const { return *mThreadPool; }
		int GetPendingJobs() const { return mPendingJobs; }

	private:

		NOUS_ThreadPool* mThreadPool;
		std::atomic<int> mPendingJobs{ 0 };
		std::mutex mWaitMutex;
		std::condition_variable mWaitCondition;

	};
}

namespace NOUS_Multithreading
{
	void JobSystemDebugInfo(const NOUS_JobSystem& system);

	static NOUS_Thread* sMainThread = nullptr;

	// Add these declarations
	void RegisterMainThread();

	void UnregisterMainThread();

	NOUS_Thread* GetMainThread();

	//void Initialize();

	//void Update();

	//void Shutdown();

	// Remember to query info from Main thread as well, but don't store it because we don't have the hold of it.
	//void NOUS_Multithreading::RegisterMainThread()
	//{
	//	NOUS_Thread* mainThread = NOUS_NEW<NOUS_Thread>(MemoryManager::MemoryTag::THREAD);

	//	mainThread->name = "Main Thread";
	//	mainThread->ID = GetThreadID(std::this_thread::get_id());
	//	mainThread->state = ThreadState::RUNNING;
	//	mainThread->executionTime.Start();

	//	RegisterThread(mainThread);
	//}
}