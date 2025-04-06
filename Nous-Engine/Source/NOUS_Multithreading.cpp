#include "NOUS_Multithreading.h"

#include "MemoryManager.h"

/// @brief Initializes the main thread tracking.
/// @note Must be paired with UnregisterMainThread() to prevent memory leaks.
void NOUS_Multithreading::RegisterMainThread()
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
void NOUS_Multithreading::UnregisterMainThread()
{
	if (sMainThread)
	{
		NOUS_DELETE<NOUS_Thread>(sMainThread, MemoryManager::MemoryTag::THREAD);
		sMainThread = nullptr;
	}
}

/// @brief Retrieves the main thread instance.
/// @return Pointer to the main thread object, or nullptr if not registered.
NOUS_Multithreading::NOUS_Thread* NOUS_Multithreading::GetMainThread()
{
	return sMainThread;
}
