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
	};

	extern std::vector<NOUS_Thread*> registeredThreads;

	uint32 GetCurrentThreadID();

	NOUS_Thread* CreateThread(const std::string& name, ThreadState initialState);
	void StartThread(NOUS_Thread* thread, std::function<void()> task);

	void RegisterMainThread();
	void Initialize();
	void Shutdown();

	void ChangeState(NOUS_Thread* thread, const ThreadState& state);

	uint32 GetThreadID(std::thread::id id);
}