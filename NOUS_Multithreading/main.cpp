#include "UnitTests.h"

int main(int argc, char** argv)
{
	NOUS_Multithreading::RegisterMainThread();

	::testing::InitGoogleTest(&argc, argv);
	RUN_ALL_TESTS();

	NOUS_Multithreading::UnregisterMainThread();

	return 0;
}

// ----------------------------------------------------------------------------------------------------

void ExampleUsage()
{
	NOUS_Multithreading::RegisterMainThread();

	// Create the job system dynamically
	NOUS_Multithreading::NOUS_JobSystem* jobSystem =
		NOUS_NEW<NOUS_Multithreading::NOUS_JobSystem>(MemoryManager::MemoryTag::THREAD);

	// Submit jobs
	jobSystem->SubmitJob([]() { /* task 1 */ }, "Task1");
	jobSystem->SubmitJob([]() { /* task 2 */ }, "Task2");

	// Wait for completion
	jobSystem->WaitForPendingJobs();

	// Clean up when done
	NOUS_DELETE<NOUS_Multithreading::NOUS_JobSystem>(jobSystem, MemoryManager::MemoryTag::THREAD);

	NOUS_Multithreading::UnregisterMainThread();
}