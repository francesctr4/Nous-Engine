#include "NOUS_Multithreading.h"

#include <iostream>

void WorkerTask() 
{
	std::lock_guard<std::mutex> lock(NOUS_Multithreading::sThreadsMutex);
	std::cout << NOUS_Multithreading::GetCurrentThreadID() << std::endl;
}

void Foo() 
{
	NOUS_Multithreading::Initialize();

	// Create and register a thread
	NOUS_Multithreading::NOUS_Thread* thread = 
		NOUS_Multithreading::CreateThread("Skibidi Thread", NOUS_Multithreading::ThreadState::READY);

	// Retrieve and use the thread handle
	NOUS_Multithreading::NOUS_Thread* retrievedThread = NOUS_Multithreading::GetThreadHandle(thread->ID);

	if (retrievedThread) 
	{
		NOUS_Multithreading::StartThread(retrievedThread, WorkerTask);
	}

	// Clean up
	NOUS_Multithreading::UnregisterThread(thread->ID);
	NOUS_Multithreading::DestroyThread(thread);

	NOUS_Multithreading::Shutdown();
}

int main() 
{
	// Main Thread
	Foo();
	//std::cout << "Main Thread" << std::endl;
	//std::cout << NOUS_Multithreading::GetCurrentThreadID() << std::endl;

	//std::cout << "Worker Threads" << std::endl;

	//std::cout << "MAX_HARDWARE_THREADS: " << NOUS_Multithreading::c_MAX_HARDWARE_THREADS << std::endl;

	//// Create Worker Thread
	//for (int i = 0; i < NOUS_Multithreading::c_MAX_HARDWARE_THREADS; ++i) 
	//{
	//	NOUS_Multithreading::NOUS_Thread* worker = NOUS_Multithreading::CreateThread(std::format("Worker Thread {}" , i + 1), NOUS_Multithreading::ThreadState::READY);
	//}

	//for (auto& [ID, thread] : NOUS_Multithreading::registeredThreads)
	//{
	//	NOUS_Multithreading::StartThread(thread, WorkerTask);
	//}

	//for (auto& [ID, thread] : NOUS_Multithreading::registeredThreads)
	//{
	//	NOUS_Multithreading::DestroyThread(thread);
	//}

	return 0;
}