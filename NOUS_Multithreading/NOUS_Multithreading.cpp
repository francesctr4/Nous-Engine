#include "NOUS_Multithreading.h"

const std::unordered_map<NOUS_Multithreading::ThreadState, std::string>
NOUS_Multithreading::NOUS_Thread::stateToString{
	{ThreadState::READY, "READY"},
	{ThreadState::RUNNING, "RUNNING"},
	{ThreadState::WAITING, "WAITING"}
};

void NOUS_Multithreading::JobSystemDebugInfo(const NOUS_JobSystem& system) 
{
	const auto& threadPool = system.GetThreadPool();
	const auto& threads = threadPool.GetThreads();

	std::cout << "===== Job System Debug Info =====\n";
	std::cout << "Pending Jobs: " << system.GetPendingJobs() << "\n";

	// Add main thread info
	if (auto* mainThread = GetMainThread()) {
		std::cout << "Thread '" << mainThread->GetName() << "' (ID: "
			<< NOUS_Thread::GetThreadID(std::this_thread::get_id()) << ") - "
			<< NOUS_Thread::GetStringFromState(mainThread->GetThreadState())
			<< ", Exec Time: " << mainThread->GetExecutionTimeMS() << "ms" << "\n";
	}

	for (const auto& thread : threads) 
	{
		std::cout << "Thread '" << thread->GetName() << "' (ID: " << thread->GetID() << ") - "
			<< thread->GetStringFromState(thread->GetThreadState()).c_str()
			<< ", Exec Time: " << thread->GetExecutionTimeMS() << "ms"
			<< ", Job: " << (thread->GetCurrentJob() ? thread->GetCurrentJob()->GetName() : "None") << "\n";
	}

	if (system.GetThreadPool().GetThreads().empty()) 
	{
		std::cout << "[Sequential Mode] Jobs executed on main thread\n";
	}
}

// Add these declarations
void NOUS_Multithreading::RegisterMainThread()
{
	if (!sMainThread) {
		sMainThread = NOUS_NEW<NOUS_Thread>(MemoryManager::MemoryTag::THREAD);
		sMainThread->SetName("Main Thread");
		sMainThread->SetThreadState(ThreadState::RUNNING);
		sMainThread->StartExecutionTimer();
	}
}

void NOUS_Multithreading::UnregisterMainThread()
{
	if (sMainThread)
	{
		NOUS_DELETE<NOUS_Thread>(sMainThread, MemoryManager::MemoryTag::THREAD);
	}
}

NOUS_Multithreading::NOUS_Thread* NOUS_Multithreading::GetMainThread()
{
	return sMainThread;
}

//void NOUS_Multithreading::Initialize()
//{
//	// Initialize job system
//
//}
//
//void NOUS_Multithreading::Update()
//{
//	// Submit jobs on update
//	for (int i = 0; i < 100; ++i) {
//		jobSystem.SubmitJob([i]() {
//			// Your task logic here
//			std::cout << "Job " << i << " executed\n";
//			});
//	}
//
//	JobSystemDebugInfo();
//}
//
//void NOUS_Multithreading::Shutdown()
//{
//	// Wait for all jobs to complete
//	jobSystem.WaitForAll();
//}
