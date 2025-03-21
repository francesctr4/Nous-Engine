#include "NOUS_Multithreading.h"

const std::unordered_map<NOUS_Multithreading::ThreadState, std::string>
NOUS_Multithreading::NOUS_Thread::stateToString{
	{ThreadState::READY, "READY"},
	{ThreadState::RUNNING, "RUNNING"},
	{ThreadState::WAITING, "WAITING"}
};

void NOUS_Multithreading::JobSystemDebugInfo()
{
	// Log the state of all the system
	const auto& threadPool = jobSystem.GetThreadPool();
	const auto& threads = threadPool.GetThreads();

	std::cout << "===== Job System Debug Info =====\n";
	std::cout << "Pending Jobs: " << jobSystem.GetPendingJobs() << "\n";
	for (const auto& thread : threads) {
		std::cout << "Thread '" << thread->GetName() << "' (ID: " << thread->GetID() << ") - ";
		std::cout << thread->GetStringFromState(thread->GetThreadState()).c_str();
		std::cout << ", Exec Time: " << thread->GetExecutionTimeMS() << "ms\n";
	}
}

void NOUS_Multithreading::Initialize()
{
	// Initialize job system

}

void NOUS_Multithreading::Update()
{
	// Submit jobs on update
	for (int i = 0; i < 100; ++i) {
		jobSystem.SubmitJob([i]() {
			// Your task logic here
			std::cout << "Job " << i << " executed\n";
			});
	}

	JobSystemDebugInfo();
}

void NOUS_Multithreading::Shutdown()
{
	// Wait for all jobs to complete
	jobSystem.WaitForAll();
}
