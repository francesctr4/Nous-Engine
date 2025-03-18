#pragma once

#include "Globals.h"

#include <thread>
#include <functional>
#include <atomic>
#include <sstream>
#include <mutex>

#include "Timer.h"

namespace NOUS_Multithreading 
{
	const uint32 c_MAX_HARDWARE_THREADS = std::thread::hardware_concurrency();
	static std::mutex sThreadsMutex;
	static std::atomic<uint32> sThreadIDCounter{ 0 };

	enum class ThreadState 
	{
		READY,
		RUNNING,
		WAITING
	};

	std::string GetStringFromState(const ThreadState& state);

	struct NOUS_Thread 
	{
		std::thread handle;
		std::function<void()> task;

		std::string name;
		uint32 ID;
		std::atomic<ThreadState> state;
		Timer executionTime;

		~NOUS_Thread() 
		{
			if (handle.joinable()) 
			{
				handle.join();
			}

			handle = std::thread();
			task = nullptr;
			name.clear();
			ID = 0;
			state.store(ThreadState::READY);
			executionTime.Stop();
		}
	};

	extern std::unordered_map<uint32,NOUS_Thread*> registeredThreads;

	uint32 GetCurrentThreadID();
	uint32 GetThreadID(std::thread::id id);

	NOUS_Thread* GetThreadHandle(uint32 threadID);

	NOUS_Thread* CreateThread(const std::string& name, ThreadState initialState);
	void StartThread(NOUS_Thread* thread, std::function<void()> task);
	void DestroyThread(NOUS_Thread* thread);

	void RegisterMainThread();
	void Initialize();
	void Shutdown();

	void ChangeState(NOUS_Thread* thread, const ThreadState& state);

	void RegisterThread(NOUS_Thread* thread);
	void UnregisterThread(uint32 threadID);
}