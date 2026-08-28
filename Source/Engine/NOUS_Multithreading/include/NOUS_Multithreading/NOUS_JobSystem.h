#pragma once

#include <EngineCore/EngineExport.h>

#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <cstddef>

namespace nous::engine::multithreading
{
	// Forward declarations
	class NOUS_ThreadPool;

	///////////////////////////////////////////////////////////////////////////
	/// @brief Maximum hardware threads available, minus one reserved for the main thread.
	///////////////////////////////////////////////////////////////////////////
	inline const uint8_t c_MAX_HARDWARE_THREADS = []
	{
			const unsigned int hardwareThreads = std::thread::hardware_concurrency();
			return hardwareThreads == 0 ? 0 : hardwareThreads - 1;
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
		NOUS_ENGINE_API explicit NOUS_JobSystem(uint8_t size = c_MAX_HARDWARE_THREADS);

		/// @brief NOUS_JobSystem destructor.
		/// @note Wait until all threads have finished their work and then delete the thread pool.
		NOUS_ENGINE_API ~NOUS_JobSystem();

		/// @brief Submits a job to the thread pool, to be executed by a free worker thread.
		/// @note Job executes immediately if thread pool size is 0 (running on Main Thread).
		/// @param userJob: The function to execute.
		/// @param jobName: Optional name identifier.
		NOUS_ENGINE_API void SubmitJob(const std::function<void()>& userJob, const std::string& jobName = "Unnamed");

		/// @brief Queues work to run on the MAIN thread, drained once per frame by
		///        Application::Update before modules update.
		/// @note Use this for anything a worker must not touch directly — above all,
		///       structural mutation of the entt registry (creating GameObjects,
		///       adding components).
		/// @warning MOVE everything the task needs into the lambda. Capturing by
		///          reference or by pointer to a temporary is a dangling read by the
		///          time the task runs — it executes on a later frame, not here.
		/// @note Deliberately NOT counted in the pending-job total: WaitForPendingJobs()
		///       is called from the main thread, which is the only thread that can drain
		///       this queue, so counting them would deadlock.
		NOUS_ENGINE_API void SubmitToMainThread(std::function<void()> task, const std::string& taskName = "Unnamed");

		/// @brief Runs and clears every queued main-thread task. Call ONLY from the main thread.
		NOUS_ENGINE_API void DrainMainThreadQueue();

		/// @brief Stops accepting main-thread tasks and discards those still queued.
		/// @note Called during shutdown. Pending work is dropped, not run: executing
		///       entity construction while modules are being torn down is worse than
		///       losing a spawn the user will never see.
		NOUS_ENGINE_API void CloseMainThreadQueue();

		/// @return Number of main-thread tasks awaiting a drain.
		[[nodiscard]] NOUS_ENGINE_API size_t GetPendingMainThreadTaskCount() const;

		/// @brief Blocks until all submitted jobs complete.
		NOUS_ENGINE_API void WaitForPendingJobs();

		/// @brief Resizes the thread pool to the specified number of threads.
		/// @param newSize: The new number of worker threads in the pool.
		/// @note If the size passed is 0, the program becomes single-threaded.
		/// @note Ensures all current jobs finish before resizing.
		NOUS_ENGINE_API void Resize(uint8_t newSize);

		/// @return Reference to the underlying thread pool.
		NOUS_ENGINE_API const NOUS_ThreadPool& GetThreadPool() const;

		/// @return Number of pending unprocessed jobs.
		NOUS_ENGINE_API int GetPendingJobs() const;

		/// @return Number of worker threads in the pool.
		NOUS_ENGINE_API size_t GetWorkerCount() const;

	private:

		NOUS_ThreadPool*			mThreadPool;
		std::atomic<int>			mPendingJobs;

		std::mutex					mWaitMutex;
		std::condition_variable		mWaitCondition;

		struct MainThreadTask
		{
			std::function<void()> fn;
			std::string           name;
		};

		mutable std::mutex          m_mainThreadMutex;
		std::vector<MainThreadTask> m_mainThreadQueue;
		bool                        m_mainThreadQueueClosed = false;

	};
}